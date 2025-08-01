#include <iostream>

#include "http/gateway_class.hpp"
#include "log/log_manager.hpp"

using namespace std;
using GatewayApp = chatroom::gateway::GatewayClass;
namespace asio = boost::asio;
namespace ip = asio::ip;
int main() {
    asio::io_context http_ctx;
    ip::tcp::endpoint ep(ip::tcp::v4(), 1234);
    GatewayApp gateway(http_ctx, ep);
    gateway.run(10);

    http_ctx.run();
    
    // FIXME(user): 优雅关闭

    return 0;
}