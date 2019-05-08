#ifndef _EASY_TCP_CLIENT_HPP_
#define _EASY_TCP_CLIENT_HPP_

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
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

#include <iostream>
#include <glog/logging.h>
#include "message_header.hpp"

class EasyTcpClient {
public:
    EasyTcpClient() {
        sock_ = INVALID_SOCKET;
    }

    virtual ~EasyTcpClient() {
        close_socket();
    }

    //初始化socket
    void init_socket() {
#ifdef _WIN32
        //@TODO
#else
        if (INVALID_SOCKET != sock_) {
            LOG(INFO) << "<socket=" << sock_ << ">关闭旧连接...";
            close_socket();
        }
        sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (INVALID_SOCKET == sock_) {
            LOG(ERROR) << "错误,建立socket失败...";
        }
        else {
            LOG(INFO) << "建立socket成功...";
        }
#endif 
    }

    //连接服务器
    int connect_to(char* ip, unsigned short port) {
        if (INVALID_SOCKET == sock_) {
            init_socket();
        }

        //连接服务器 connect
        sockaddr_in _sin = {};
        _sin.sin_family = AF_INET;
        _sin.sin_port = htons(port);
#ifdef _WIN32
        //@TODO
#else
        _sin.sin_addr.s_addr = inet_addr(ip);
#endif 
        int ret = connect(sock_, (sockaddr*)&_sin, sizeof(sockaddr_in));
        if (SOCKET_ERROR == ret) {
            LOG(ERROR) << "错误,连接服务器失败...";
        }
        else {
            LOG(INFO) << "连接服务器成功...";
        }
        return ret;
    }

    //关闭套接字
    void close_socket() {
         if (sock_ != INVALID_SOCKET) {
#ifdef _WIN32
            //@TODO
#else
            close(sock_);
#endif 
            sock_ = INVALID_SOCKET;
         }
    }

    //处理网络消息
    bool on_run() {
        if (is_run()) {
            fd_set fd_reads;
            FD_ZERO(&fd_reads);
            FD_SET(sock_, &fd_reads);
            timeval t = {1, 0};
            int ret = select(sock_ + 1, &fd_reads, 0, 0, &t);
            if (ret < 0) {
                LOG(INFO) << "<socket=" << sock_ << ">任务结束1.";
                return false;
            }
            if (FD_ISSET(sock_, &fd_reads)) {
                FD_CLR(sock_, &fd_reads);
                if (-1 == recv_data(sock_)) {
                    LOG(INFO) << "<socket=" << sock_ << ">select任务结束2.";
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    //是否工作中
    bool is_run() {
        return (sock_ != INVALID_SOCKET);
    }

    //接收数据 处理粘包 拆分包
    int recv_data(SOCKET sock) {
        //缓冲区
        char recv_buf[4096] = {};
        //接收服务端数据
        //先接收数据头:
        int n_len = (int)recv(sock_, recv_buf, sizeof(DataHeader), 0);
        DataHeader* header = (DataHeader*)recv_buf;
        LOG(INFO) << "recv_data: n_len=" << n_len;
        if (n_len <= 0) {
            LOG(INFO) << "与服务器断开连接,任务结束.";
            return -1;
        }
        //接收数据体:
        recv(sock_, recv_buf + sizeof(DataHeader), header->data_length - sizeof(DataHeader), 0);
        on_net_msg(header);
        return 0;
    }

    //响应网络消息
    //传入参数为一个数据包
    void on_net_msg(DataHeader* header) {
        switch (header->cmd) {
        case CMD_LOGIN_RESULT: {
            LoginResult* login = (LoginResult*)header;
            LOG(INFO) << "收到服务端消息:CMD_LOGIN_RESULT,数据长度:"
                      << login->data_length;
        }
            break;
        case CMD_LOGOUT_RESULT: {
            LogoutResult* logout = (LogoutResult*)header;
            LOG(INFO) << "收到服务端消息:CMD_LOGOUT_RESULT,数据长度:"
                      << logout->data_length;
        }
            break; 
        case CMD_NEW_USER_JOIN: {
            NewUserJoin* user_join = (NewUserJoin*)header;
            LOG(INFO) << "收到服务端消息:CMD_NEW_USER_JOIN,数据长度:"
                      << user_join->data_length;
        }
            break;       
        default:
            break;
        }
    }

    //发送数据
    int send_data(DataHeader* header) {
        if (is_run() && header) {
            return send(sock_, (const char*)header, header->data_length, 0);
        }
        return SOCKET_ERROR;
    }
private:
    SOCKET sock_;
};

#endif