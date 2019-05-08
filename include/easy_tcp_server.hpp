#ifndef _EASY_TCP_SERVER_HPP_
#define _EASY_TCP_SERVER_HPP_

#ifdef _WIN32
    #define FD_SETSIZE 2506
    #define WIN32_LEAN_AND_MEAN
    #define _WINSOCK_DEPRECATED_NO_WARNING
    #include <windows.h>
    #include <WinSock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else 
    #include <unistd.h> //unix std
    #include <arpa/inet.h>
    #include <string.h>

    #define SOCKET int
    #define INVALID_SOCKET (SOCKET)(~0)
    #define SOCKET_ERROR (-1)
#endif

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <glog/logging.h>

#include "message_header.hpp"
#include "cell_time_stamp.hpp"

//缓冲区最小单元大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240 //10k
#endif

#define CELL_SERVER_THREAD_COUNT 4

class ClientSocket {
public:
    ClientSocket(SOCKET sockfd = INVALID_SOCKET) {
        sockfd_ = sockfd;
        memset(msg_buff_, 0, sizeof(msg_buff_));
        last_pos_ = 0;
    }

    SOCKET sockfd() {
        return sockfd_;
    }

    char* msg_buff() {
        return msg_buff_;
    }

    int get_last_pos() {
        return last_pos_;
    }

    void set_last_pos(int pos) {
        last_pos_ = pos;
    }

private:
    //socket fd_set file desc set
    SOCKET sockfd_;
    //第二缓冲区 消息缓冲区
    char msg_buff_[RECV_BUFF_SIZE * 10];
    //消息缓冲区的数据尾部位置
    int last_pos_;
};

class INetEvent {
public:
    //客户端离开事件
    virtual void on_leave(ClientSocket* client) = 0;
    virtual void on_net_msg(SOCKET sock, DataHeader* header) = 0;
private:
};

class CellServer {
public:
    CellServer(SOCKET sock = INVALID_SOCKET) {
        sock_ = sock;
        thread_ = nullptr;
        recv_count_ = 0;
        net_event_ = nullptr;
    }

    ~CellServer() {
        close_socket();
        sock_ = INVALID_SOCKET;
    }

    void set_event_obj(INetEvent* event) {
        net_event_ = event;
    }

    //关闭socket
    void close_socket() {
        if (sock_ != INVALID_SOCKET) {
#ifdef _WIN32
            //@TODO
#else
            for (int n = (int)clients_.size() - 1; n >= 0; n--) {
                close(clients_[n]->sockfd());
                delete clients_[n];
            }
            //关闭套接字
            close(sock_);
#endif
            clients_.clear();
        }
    }

    //是否工作中
    bool is_run() {
        return (sock_ != INVALID_SOCKET);
    }

    //处理网络消息
    bool on_run() {
        while (is_run()) {
            if (clients_buff_.size() > 0) {
                //从缓冲队列里取出客户数据
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto client : clients_buff_) {
                    clients_.push_back(client);
                }
                clients_buff_.clear();
            }

            //如果没有需要处理的客户端，就跳过
            if (clients_.empty()) {
                std::chrono::milliseconds t(1);
                std::this_thread::sleep_for(t);
                continue;
            }

            //伯克利套接字 BSD socket
            fd_set fd_read; //socket 集合
            //清理集合
            FD_ZERO(&fd_read);
            //将socket加入集合
            SOCKET max_sock = clients_[0]->sockfd();
            for (int n = (int)clients_.size() - 1; n >= 0; n--) {
                FD_SET(clients_[n]->sockfd(), &fd_read); //加入
                if (max_sock < clients_[n]->sockfd()) {
                    max_sock = clients_[n]->sockfd();
                }
            }

            //nfds是一个整数值, 是指fd_set集合中所有socket的范围,而不是数量
            //即是所有socket最大值+1, 在windows中这个参数可以写0
            int ret = select(max_sock + 1, &fd_read, nullptr, nullptr, nullptr);
            if (ret < 0) {
                //loginfo("selete任务结束.\n");
                LOG(INFO) << "selete任务结束.";
                close_socket();
                return false;
            }

            for (int n = (int)clients_.size() - 1; n >= 0; n--) {
                if (FD_ISSET(clients_[n]->sockfd(), &fd_read)) {
                    if (-1 == recv_data(clients_[n])) {
                        auto iter = clients_.begin() + n;
                        if (iter != clients_.end()) {
                            if (net_event_) {
                                net_event_->on_leave(clients_[n]);
                            }
                            delete clients_[n];
                            clients_.erase(iter);
                        }
                    }
                }
            }
        }
    }

    //缓冲区
    char recv_buff[RECV_BUFF_SIZE] ={};
    //接收数据 处理粘包 拆分包
    int recv_data(ClientSocket* client) {
        //接收客户端数据
        int n_len = (int)recv(client->sockfd(), recv_buff, RECV_BUFF_SIZE, 0);
        if (n_len <= 0) {
            LOG(INFO) << "客户端<socket=" << client->sockfd() << ">已退出，任务结束.";
            return -1;
        }

        //将接收到的数据拷贝到消息缓冲区
        memcpy(client->msg_buff() + client->get_last_pos(), recv_buff, n_len);
        //消息缓冲区的数据尾部位置后移
        client->set_last_pos(client->get_last_pos() + n_len);

        //判断消息缓冲区的数据长度大于消息头DataHeader长度
        while (client->get_last_pos() >= sizeof(DataHeader)) {
            DataHeader* header = (DataHeader*)client->msg_buff();
            //判断消息缓冲区的数据长度大于消息长度
            if (client->get_last_pos() >= header->data_length) {
                //消息缓冲区剩余未处理数据的长度
                int n_size = client->get_last_pos() - header->data_length;
                //处理网络消息
                on_net_msg(client->sockfd(), header);
                //将消息缓冲区剩余未处理数据前移
                memcpy(client->msg_buff(), client->msg_buff() + header->data_length, n_size);
                //消息缓冲区的数据尾部位置前移
                client->set_last_pos(n_size);
            }
            else {
                //消息缓冲区剩余数据不够一条完整消息
                break;
            }
        }
        return 0;
    }

    //响应网络消息
    virtual void on_net_msg(SOCKET sock, DataHeader* header) {
        recv_count_++;
        net_event_->on_net_msg(sock, header);
        switch (header->cmd) {
        case CMD_LOGIN: {
            Login* login = (Login*)header;
            LOG(INFO) << "收到客户端<socket=" << sock 
                      << ">请求:CMD_LOGIN,数据长度:" << login->data_length
                      << "user_name:" << login->user_name
                      << "password:" << login->password;
                     
        }
            break;
        case CMD_LOGOUT: {
            Logout* logout = (Logout*)header;
            LOG(INFO) << "收到客户端<socket=" << sock 
                      << ">请求:CMD_LOGOUT,数据长度:" << logout->data_length
                      << "user_name:" << logout->user_name;
        }
            break;
        default: {
            LOG(INFO) << "<socket=" << sock 
                      << ">收到未定义消息,数据长度:" << header->data_length;
        }
            break;
        }
    }

    void add_client(ClientSocket* client) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_buff_.push_back(client);
    }

    void start() {
        thread_ = new std::thread(std::mem_fun(&CellServer::on_run), this);
    }

    size_t get_client_count() {
        return clients_.size() + clients_buff_.size();
    }

public:
    std::atomic_int recv_count_;
private:
    SOCKET sock_;
    //正式客户队列
    std::vector<ClientSocket*> clients_;
    //缓冲客户队列
    std::vector<ClientSocket*> clients_buff_;
    std::mutex mutex_;
    std::thread* thread_;
    INetEvent* net_event_;
};

class EasyTcpServer : public INetEvent {
public:
    EasyTcpServer() {
        sock_ = INVALID_SOCKET;
    }

    virtual ~EasyTcpServer() {
        close_socket();
    }

    //初始化socket
    SOCKET init_socket() {
#ifdef _WIN32 
        //@TODO
#endif 
        if (INVALID_SOCKET != sock_) {
            LOG(INFO) << "<socket:" << (int)sock_ << ">关闭旧连接...";
            close_socket();
        }
        sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (INVALID_SOCKET == sock_) {
            LOG(ERROR) << "错误,建立socket失败...";
        }
        else {
            LOG(INFO) << "建立socket:" << (int)sock_ << "成功.";
        }
        return sock_;
    }

    //绑定ip和端口
    int bind_port(const char* ip, unsigned short port) {
        sockaddr_in _sin = {};
        _sin.sin_family = AF_INET;
        _sin.sin_port = htons(port); //host to net unsigned short
#ifdef _WIN32 
        //@TODO
#else 
        if (ip) {
            _sin.sin_addr.s_addr = inet_addr(ip);
        }
        else {
            _sin.sin_addr.s_addr = INADDR_ANY;
        }
#endif
        int ret = bind(sock_, (sockaddr*)&_sin, sizeof(_sin));
        if (SOCKET_ERROR == ret) {
            LOG(ERROR) << "错误,绑定网络端口<" << port << ">失败...";
        }
        else {
            LOG(INFO) << "绑定网络端口<" << port << ">成功.";
        }
        return ret;
    }

    //监听端口号
    int server_listen(int n) {
        //监听网络端口
        int ret = listen(sock_, n);
        if (SOCKET_ERROR == ret) {
            LOG(ERROR) << "<socket:" << sock_ << ">错误,监听网络端口失败...";
        }
        else {
            LOG(INFO) << "<socket:" << sock_ << ">监听网络端口成功.";
        }
        return ret;
    }

    //接收客户端连接
    SOCKET accept_client() {
        //accept 等待接受客户端连接
        sockaddr_in client_addr = {};
        int addr_len = sizeof(sockaddr_in);
        SOCKET sock = INVALID_SOCKET;
#ifdef _WIN32
        //@TODO
#else 
        sock = accept(sock_, (sockaddr*)&client_addr, (socklen_t*)&addr_len);  
#endif 
        if (INVALID_SOCKET == sock) {
            LOG(ERROR) << "socket=<" << (int)sock_ << ">错误,接受到无效客户端SOCKET.";
        }
        else {
            add_client_to_cell_server(new ClientSocket(sock));
        }
    }

    void add_client_to_cell_server(ClientSocket* client) {
        clients_.push_back(client);
        //查找客户数量最少的CellServer消息处理对象
        auto min_server = cell_servers_[0];
        for (auto cell_server : cell_servers_) {
            if (min_server->get_client_count() > cell_server->get_client_count()) {
                min_server = cell_server;
            }
        }
        min_server->add_client(client);
    }

    void start() {
        for (int n = 0; n < CELL_SERVER_THREAD_COUNT; n++) {
            auto ser = new CellServer(sock_);
            cell_servers_.push_back(ser);
            ser->set_event_obj(this);
            ser->start();
        }
    }

    //关闭socket
    void close_socket() {
        if (sock_ != INVALID_SOCKET) {
#ifdef _WIN32 
            //@TODO
#else 
            for (int n = (int)clients_.size() - 1; n >= 0; n--) {
                close(clients_[n]->sockfd());
                delete clients_[n];
            }
            //关闭套接字
            close(sock_);
#endif 
            clients_.clear();
        }
    }

    //处理网络消息
    bool on_run() {
        if (is_run()) {
            time_for_msg();

            fd_set fd_read;
            FD_ZERO(&fd_read);
            FD_SET(sock_, &fd_read);
            timeval t = {0, 10};
            int ret = select(sock_ + 1, &fd_read, 0, 0, &t);
            if (ret < 0) {
                LOG(INFO) << "selete任务结束.";
                close_socket();
                return false;
            }
            //判断socket是否在集合中
            if (FD_ISSET(sock_, &fd_read)) {
                FD_CLR(sock_, &fd_read);
                accept_client();
                return true;
            }
            return true;
        }
        return false;
    }

    //是否工作中
    bool is_run() {
        return (sock_ != INVALID_SOCKET);
    }

    //响应网络消息
    void time_for_msg() {
        auto t1 = time_.get_elapsed_second();
        if (t1 >= 1.0) {
            int recv_count = 0;
            for (auto ser : cell_servers_) {
                recv_count += ser->recv_count_;
                ser->recv_count_ = 0;
            }
            LOG(INFO) << "Thread<" << cell_servers_.size()
                      << "> time<" << t1 
                      << "> socket<" << sock_
                      << "> clients<" << (int)clients_.size()
                      << "> recv_count<" << (int)(recv_count / t1);
            time_.update();
        }
    }

    //发送指定socket数据
    int send_data(SOCKET sock, DataHeader* header) {
        if (is_run() && header) {
            return send(sock_, (const char*)header, header->data_length, 0);
        }
        return SOCKET_ERROR;
    }

    void send_data_to_all(DataHeader* header) {
        for (int n = (int)clients_.size() - 1; n >= 0; n--) {
            send_data(clients_[n]->sockfd(), header);
        }
    }

    virtual void on_leave(ClientSocket* client) {
        for (int n = (int)clients_.size() - 1; n >= 0; n--) {
            if (clients_[n] == client) {
                auto iter = clients_.begin() + n;
                if (iter != clients_.end()) {
                    clients_.erase(iter);
                }
            }
        }
    }

    virtual void on_net_msg(SOCKET sock, DataHeader* header) {

    }

private:
    SOCKET sock_;
    std::vector<ClientSocket*> clients_;
    std::vector<CellServer*> cell_servers_;
    CELLTimestamp time_;
};

#endif