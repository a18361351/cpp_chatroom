#include <sw/redis++/connection.h>

#include <string>

#include "log/log_manager.hpp"
#include "status/status_class.hpp"

// TODO(user): 为各个组件做一个读取config的功能

const std::string STATUS_ADDR = "0.0.0.0:3000";

int main(int argc, char **argv) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Status server executable starting at {}", STATUS_ADDR.c_str());
    chatroom::status::StatusServer srv;
    // redis service
    sw::redis::ConnectionOptions conn_opt;  // 连接到的服务器选项
    conn_opt.host = "192.168.56.101";
    conn_opt.port = 6379;
    conn_opt.password = "sword";
    conn_opt.db = 0;
    conn_opt.socket_timeout = std::chrono::milliseconds(200);  // 200ms

    sw::redis::ConnectionPoolOptions pool_opt;  // 连接池选项
    pool_opt.size = 3;                          // 连接池中最大连接数
    pool_opt.connection_lifetime = std::chrono::minutes(10);  // 连接的最大生命时长，超过时长连接会过期并重新建立

    srv.RunStatusServer(STATUS_ADDR, conn_opt, pool_opt);
    return 0;
}