#include "easy_tcp_client.hpp"
#include <thread>

void cmd_thread(EasyTcpClient* client) {
    while (true) {
        char cmd_buf[256] = {};
        std::cin >> cmd_buf;
        if (0 == strcmp(cmd_buf, "exit")) {
            client->close_socket();
            LOG(INFO) << "退出cmd_thread线程.";
            break;
        }
        else if (0 == strcmp(cmd_buf, "login")) {
            Login login;
            strcpy(login.user_name, "ssum");
            strcpy(login.password, "ssumxxx");
            client->send_data(&login);
        }
        else if (0 == strcmp(cmd_buf, "logout")) {
            Logout logout;
            strcpy(logout.user_name, "ssum");
            client->send_data(&logout);
        }
        else {
            LOG(WARNING) << "不支持的命令.";
        }
    }
}

int main(int argc, char** argv) {
    FLAGS_log_dir = "../log/";
    google::InitGoogleLogging(argv[0]);
    FLAGS_colorlogtostderr = true;
    FLAGS_max_log_size = 1; //最大1M
    FLAGS_stop_logging_if_full_disk = true;
    FLAGS_alsologtostderr = true;

    EasyTcpClient client;
    client.connect_to("127.0.0.1", 4567);

    //启动输入线程
    std::thread th(cmd_thread, &client);
    th.detach();

    int i = 0;
    while (client.is_run()) {
        // LOG(INFO) << "running:" << i++;
        client.on_run();
    }

    client.close_socket();
    LOG(INFO) << "已退出.";
    return 0;
}