#include <iostream>

#include <boost/asio.hpp>

#include "server/server_class.hpp"
#include "server/session.hpp"
#include "utils/util_class.hpp"

using namespace std;
using namespace boost::asio;

const uint32_t server_id = 100;
const string status_rpc_addr = "192.168.56.101:3000";
const uint32_t listen_port = 1235;
const string server_addr = std::string("192.168.56.101:") + std::to_string(listen_port);

int main() {
    try {
        boost::asio::io_context ctx;
        // 实现程序的优雅退出，监听SIGINT和SIGTERM
        boost::asio::signal_set sigset(ctx, SIGINT, SIGTERM);
        

        // 指定本地端口
        ip::tcp::endpoint local(ip::tcp::v4(), listen_port);

        // redis service
        sw::redis::ConnectionOptions conn_opt; // 连接到的服务器选项
        conn_opt.host = "192.168.56.101";
        conn_opt.port = 6379;
        conn_opt.password = "sword";
        conn_opt.db = 0;
        conn_opt.socket_timeout = std::chrono::milliseconds(200); // 200ms

        sw::redis::ConnectionPoolOptions pool_opt; // 连接池选项
        pool_opt.size = 3;  // 连接池中最大连接数
        pool_opt.connection_lifetime = std::chrono::minutes(10);    // 连接的最大生命时长，超过时长连接会过期并重新建立
        

        chatroom::backend::ServerClass srv(server_id, server_addr, ctx, status_rpc_addr, conn_opt, pool_opt);
        
        
        sigset.async_wait([&ctx, &srv](const errcode& err, int sig) {
            // 收到了信号
            printf("Stopping\n");
            ctx.stop();
        });
        
        srv.Listen(local);  // 监听


        spdlog::info("Server running at port 1234");
        ctx.run();  // 运行

    } catch (std::exception& e) {
        spdlog::error("Exception: {}", e.what());
    }
}