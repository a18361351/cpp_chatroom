#ifndef HTTP_SERVER_HEADER
#define HTTP_SERVER_HEADER

// http网关源码大幅参考了以下资料：
// https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/examples.html#beast.examples.servers
// https://llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF#!aid/2RlhDCg4eedYme46C6ddo4cKcFN

#include <cstddef>
#include <memory>

#include <boost/beast.hpp>
boost::beast::http::message_generator request_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);


class HTTPConnection : public std::enable_shared_from_this<HTTPConnection> {
public:
    // 启动一个HTTPConnection的执行
    void start() {
        read_request();
        check_deadline();
    }

    void read_request();
    void send_response(boost::beast::http::message_generator&& msg);
    void check_deadline();
    
    // 关闭一个HTTPConnection的连接
    void close() {
        // Send a TCP shutdown
        boost::beast::error_code ec;
        sock_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
    }
private:
    boost::beast::tcp_stream sock_;
    boost::beast::flat_buffer buf_{8192};
    boost::beast::http::request<boost::beast::http::string_body> req_;
};


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
        }
        self->send_response(request_handler(std::move(self->req_)));
    };
    // 清空请求体
    req_ = {};

    // 设置超时
    sock_.expires_after(std::chrono::seconds(30));
    
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

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// 解析请求的部分
boost::beast::http::message_generator request_handler(http::request<boost::beast::http::string_body>&& req) {
    // Returns a bad request response
    auto const bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    switch(req.method()) {
        case http::verb::get:
            return get_handler(std::move(req));
            // break;
        case http::verb::head:

            break;
        case http::verb::post:
        
            break;
        default:
            return bad_request("Unsupported HTTP-method for this server");
    }

}

http::response<http::string_body> get_handler(http::request<boost::beast::http::string_body>&& req) {
    // TODO(user): 这些lambda应该是公用的……
    // Returns a not found response
    auto const not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };
    // GET METHOD
    http::response<http::string_body> resp;
    if (req.target() == "/test") {
        resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        resp.set(http::field::content_type, "text/html");
        resp.keep_alive(req.keep_alive());
        resp.body() = "Test";
        resp.prepare_payload();
        return resp;
    } else if (req.target() == "/hello") {
        resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        resp.set(http::field::content_type, "text/html");
        resp.keep_alive(req.keep_alive());
        resp.body() = "Hello World!";
        resp.prepare_payload();
        return resp;
    } else {
        return not_found(req.target());
    }
}

// TODO(user): 可以把处理请求的逻辑分离出来

void http_server() {
    // TODO(user): 补充启动时的源码
}

#endif