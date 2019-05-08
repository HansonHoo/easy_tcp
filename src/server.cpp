#include "easy_tcp_server.hpp"
#include <thread>
#include <iostream>
#include <glog/logging.h>

bool run = true;

void cmd_thread() {
    while (true) {
        char cmd_buff[256] = {};
        std::cin >> cmd_buff;
        if (0 == strcmp(cmd_buff, "exit")) {
            run = false;
            LOG(INFO) << "退出cmd_thread线程.";
            break;
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
    FLAGS_max_log_size = 10;
    FLAGS_stop_logging_if_full_disk = true;
    FLAGS_alsologtostderr = true;
    
    LOG(INFO) << "system start...";
    EasyTcpServer server;
    server.init_socket();
    server.bind_port(nullptr, 4567);
    server.server_listen(5);
    server.start();

    //启动处理线程
    std::thread th(cmd_thread);
    th.detach();

    while (run) {
        server.on_run();
    }

    server.close_socket();
    LOG(INFO) << "已退出.";
    google::ShutdownGoogleLogging();
    return 0;
}