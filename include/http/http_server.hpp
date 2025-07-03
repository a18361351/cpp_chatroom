#ifndef HTTP_SERVER_HEADER
#define HTTP_SERVER_HEADER

// http网关源码大幅参考了以下资料：
// https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/examples.html#beast.examples.servers
// https://llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF#!aid/2RlhDCg4eedYme46C6ddo4cKcFN

#include <cstddef>
#include <memory>

#include <boost/beast.hpp>
#include <boost/asio.hpp>
boost::beast::http::message_generator request_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);


class HTTPConnection : public std::enable_shared_from_this<HTTPConnection> {
    friend class HTTPServer;
    public:
    // 启动一个HTTPConnection的执行
    void start() {
        read_request();
        // check_deadline();
    }

    void read_request();
    void send_response(boost::beast::http::message_generator&& msg);
    // void check_deadline();
    
    // 关闭一个HTTPConnection的连接
    void close() {
        // Send a TCP shutdown
        boost::beast::error_code ec;
        sock_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
    }
    
    // Ctors
    HTTPConnection(boost::asio::io_context& ctx) :
        sock_(ctx) {

    }

    private:
    boost::beast::tcp_stream sock_;
    boost::beast::flat_buffer buf_{8192};
    boost::beast::http::request<boost::beast::http::string_body> req_;
};

class HTTPServer : public std::enable_shared_from_this<HTTPServer> {
    public:
    HTTPServer(boost::asio::io_context& ctx, boost::asio::ip::tcp::endpoint ep) :
        ctx_(ctx), 
        acc_(boost::asio::make_strand(ctx)) 
        {
        boost::system::error_code err;
        // Open acceptor
        acc_.open(ep.protocol(), err);
        if (err) {
            throw std::runtime_error("Failed to open acceptor: " + err.message());
        }
        // soreuseaddr
        acc_.set_option(boost::asio::socket_base::reuse_address(true), err);
        if (err) {
            throw std::runtime_error("Failed to set reuse_address: " + err.message());
        }
        // Bind
        acc_.bind(ep, err);
        if (err) {
            throw std::runtime_error("Failed to bind acceptor: " + err.message());
        }
        // Start listening
        acc_.listen(boost::asio::socket_base::max_listen_connections, err);
        if (err) {
            throw std::runtime_error("Failed to listen on acceptor: " + err.message());
        }
    }

    // @brief 开始运行服务器，具体来说是开始接受新连接
    void start() {
        acceptor();
    }

    private:
    void acceptor() {
        std::shared_ptr<HTTPConnection> sess = std::make_shared<HTTPConnection>(ctx_);
        auto cb = [sess, self = shared_from_this()](const boost::system::error_code& err) {
            if (err) {
                // tell that error!
                fprintf(stderr, "HTTP acceptor received an error: %s\n", err.message().c_str());
                return;
            }
            sess->start();
            self->acceptor();
        };
        acc_.async_accept(sess->sock_.socket(), cb);        
    }
    boost::asio::io_context& ctx_;
    boost::asio::ip::tcp::acceptor acc_;

};


#endif