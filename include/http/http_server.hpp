#ifndef HTTP_SERVER_HEADER
#define HTTP_SERVER_HEADER

// http网关源码负责登录验证+负载均衡
// 大幅参考了以下资料：
// https://www.boost.org/doc/libs/latest/libs/beast/doc/html/beast/examples.html#beast.examples.servers
// https://llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF#!aid/2RlhDCg4eedYme46C6ddo4cKcFN

#include <cstddef>
#include <memory>

// #include <boost/beast.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/core.hpp>

// #include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <utility>

#include "http/req_handler.hpp"

// TODO(user): namespace

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
    void close();
    
    // Ctors
    HTTPConnection(boost::asio::io_context& ctx, std::shared_ptr<ReqHandler> handler) :
        sock_(ctx), handler_(std::move(handler)) {

    }

    private:
    boost::beast::tcp_stream sock_;
    boost::beast::flat_buffer buf_{8192};
    boost::beast::http::request<boost::beast::http::string_body> req_;
    std::shared_ptr<ReqHandler> handler_;
};

class HTTPServer : public std::enable_shared_from_this<HTTPServer> {
    public:
    // ctor
    // @warning HTTPServer本身不负责DBM的启动与关闭，在Start()之前DBM应该是启动好了的
    HTTPServer(boost::asio::io_context& ctx, const boost::asio::ip::tcp::endpoint& ep, std::shared_ptr<DBM> dbm);

    // @brief 开始运行服务器，具体来说是开始接受新连接
    void Start() {
        spdlog::info("HTTP server started");
        acceptor();
        
    }

    // @brief 停止服务器运行，关闭监听端口
    void Stop();

    private:
    void acceptor();
    boost::asio::io_context& ctx_;
    boost::asio::ip::tcp::acceptor acc_;
    std::shared_ptr<ReqHandler> req_handler_;
};


#endif