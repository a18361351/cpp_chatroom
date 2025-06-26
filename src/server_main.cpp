#include <iostream>

#include <boost/asio.hpp>

#include "server_class.hpp"
#include "session.hpp"
#include "util_class.hpp"

using namespace std;
using namespace boost::asio;

int main() {
    try {
        boost::asio::io_context ctx;
        // 实现程序的优雅退出，监听SIGINT和SIGTERM
        boost::asio::signal_set sigset(ctx, SIGINT, SIGTERM);
        
        sigset.async_wait([&ctx](const errcode& err, int sig) {
            // 收到了信号
            ctx.stop();
        });

        // 指定本地端口
        ip::tcp::endpoint local(ip::tcp::v4(), 1234);

        Server srv(ctx);
        srv.Listen(local);  // 监听

        ctx.run();  // 运行

    } catch (std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
}