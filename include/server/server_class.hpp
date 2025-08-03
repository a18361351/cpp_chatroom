#ifndef SERVER_CLASS_HEADER
#define SERVER_CLASS_HEADER

// 后台服务器类型Server的声明头文件

#include <unordered_map>

#ifdef USING_IOCONTEXT_POOL
#include "server/io_context_pool.hpp"
#elif defined(USING_IOTHREAD_POOL)
#include "server/io_thread_pool.hpp"
#endif
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>

using errcode = boost::system::error_code;

class Session;

class Server {
public:
    friend class Session;
    explicit Server(boost::asio::io_context& context) : context_(context), acc_(context) {}

    void Listen(const boost::asio::ip::tcp::endpoint& ep) {
        acc_.open(boost::asio::ip::tcp::v4());
        acc_.bind(ep);  // 
        acc_.listen();  // default backlog
    }

    void StartAccept();
    
    // 强制下线逻辑
    bool Kick(const std::string& uuid);

    ~Server() { // NOLINT
#ifdef USING_IOCONTEXT_POOL
    Singleton<IOContextPool>::GetInstance().Stop();
#elif defined(USING_IOTHREAD_POOL)
    Singleton<IOThreadPool>::GetInstance().Stop();
#endif
    }
private:

    bool RemoveSession(const std::string& uuid);

    bool running{true};
    // 通过sessions_储存这些会话的智能指针，保持其不被销毁
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;

    // 在使用了IOThreadPool或者IOContextPool的情况下该context_只是用于acceptor的
    boost::asio::io_context& context_;
    boost::asio::ip::tcp::acceptor acc_;
};

#endif