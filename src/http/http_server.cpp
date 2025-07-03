#include "http/http_server.hpp"
#include "http/req_handler.hpp"

using namespace std;
using namespace boost::asio;

// 异步读取请求
void HTTPConnection::read_request() {
    // 回调
    auto cb = [self = shared_from_this()](const boost::beast::error_code& err, std::size_t bytes) {
        boost::ignore_unused(bytes);
        if (err) {
            if (err == boost::beast::http::error::end_of_stream) {
                self->close();
            } else {
                // error!
                fprintf(stderr, "HTTP read error: %s\n", err.message().c_str());
                self->close();
            }
            return;
        }
        self->send_response(request_handler(std::move(self->req_)));
    };
    // 清空请求体
    req_ = {};

    // 设置超时
    // sock_.expires_after(std::chrono::seconds(30));
    
    // 读取
    boost::beast::http::async_read(sock_, buf_, req_, cb);
}

// 异步发送请求
void HTTPConnection::send_response(boost::beast::http::message_generator&& msg) {
    auto cb = [self = shared_from_this()](boost::beast::error_code err, std::size_t bytes) {
        if (err) {
            fprintf(stderr, "HTTP write error: %s\n", err.message().c_str());
            self->close();
            return;
        }
        self->read_request();
    };
    boost::beast::async_write(sock_, std::move(msg), cb);
}

int main() {
    io_context ctx;
    ip::tcp::endpoint ep(ip::tcp::v4(), 1235);
    auto srv = make_shared<HTTPServer>(ctx, ep);

    srv->start();

    ctx.run();
}