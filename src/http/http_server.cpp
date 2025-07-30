
#include "log/log_manager.hpp"
#include "http/http_server.hpp"
#include "http/req_handler.hpp"

#include <boost/asio/strand.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/read.hpp>

using namespace std;
using namespace boost::asio;

// HTTPServer class
HTTPServer::HTTPServer(boost::asio::io_context& ctx, const boost::asio::ip::tcp::endpoint& ep, std::shared_ptr<DBM> dbm) :
            ctx_(ctx), 
            acc_(boost::asio::make_strand(ctx)),
            req_handler_(std::make_shared<ReqHandler>(std::move(dbm))) {
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
void HTTPServer::acceptor() {
    std::shared_ptr<HTTPConnection> sess = std::make_shared<HTTPConnection>(ctx_, req_handler_);
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

void HTTPServer::Stop() {
    acc_.close();
    // FIXME(user): 现有的连接怎么关闭？
}

// HTTPConnection class
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

        // 回调嵌回调……
        // TODO(user): 改用协程
        auto send_cb = [self] (boost::beast::http::message_generator &&msg, bool) -> bool {
            self->send_response(std::move(msg));
            return true;
        };
        self->handler_->PostRequest(std::move(self->req_), send_cb);
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

void HTTPConnection::close() {
    // Send a TCP shutdown
    boost::beast::error_code ec;
    sock_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}