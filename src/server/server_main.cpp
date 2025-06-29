#include <iostream>

#include <boost/asio.hpp>

#include "server/server_class.hpp"
#include "server/session.hpp"
#include "utils/util_class.hpp"

using namespace std;
using namespace boost::asio;

int main() {
#ifdef USING_IOCONTEXT_POOL
    printf("Using IOContext Pool now\n");
#elif defined(USING_IOTHREAD_POOL)
    printf("Using IOThread Pool now\n");
#endif
    try {
        boost::asio::io_context ctx;
        // 实现程序的优雅退出，监听SIGINT和SIGTERM
        boost::asio::signal_set sigset(ctx, SIGINT, SIGTERM);
        
        sigset.async_wait([&ctx](const errcode& err, int sig) {
            // 收到了信号
            printf("Stopping\n");
            ctx.stop();
        });

        // 指定本地端口
        ip::tcp::endpoint local(ip::tcp::v4(), 1234);

        Server srv(ctx);
        srv.Listen(local);  // 监听
        srv.StartAccept();

        printf("Server running\n");
        ctx.run();  // 运行

    } catch (std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
}