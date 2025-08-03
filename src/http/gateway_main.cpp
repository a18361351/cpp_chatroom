#include <iostream>
#include <sw/redis++/connection.h>

#include "http/gateway_class.hpp"
#include "log/log_manager.hpp"

using namespace std;
using GatewayApp = chatroom::gateway::GatewayClass;
namespace asio = boost::asio;
namespace ip = asio::ip;
int main() {
    asio::io_context http_ctx;
    ip::tcp::endpoint ep(ip::tcp::v4(), 1234);
    const string status_ep = "192.168.56.101:3000";
    GatewayApp gateway(http_ctx);

    // redis opts
    sw::redis::ConnectionOptions conn_opt;
    // 连接到的服务器选项
    conn_opt.host = "192.168.56.101";
    conn_opt.port = 6379;
    conn_opt.password = "sword";
    conn_opt.db = 0;
    conn_opt.socket_timeout = std::chrono::milliseconds(200); // 200ms

    // 连接池选项
    sw::redis::ConnectionPoolOptions pool_opt;
    pool_opt.size = 3;  // 连接池中最大连接数
    // pool_opt.wait_timeout = std::chrono::milliseconds(100); // 当所有连接不空闲时，新的请求应该等待多久？默认是永久等待
    pool_opt.connection_lifetime = std::chrono::minutes(10);    // 连接的最大生命时长，超过时长连接会过期并重新建立
    gateway.Initialize(ep, conn_opt, pool_opt, status_ep);

    gateway.Run(10);

    http_ctx.run();
    
    // FIXME(user): 优雅关闭

    return 0;
}