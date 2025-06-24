#ifndef SERVER_CLASS_HEADER
#define SERVER_CLASS_HEADER

#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>

using errcode = boost::system::error_code;

class Session;

class Server {
public:
    friend class Session;
    Server(boost::asio::io_context& context) : context_(context), acc_(context) {}

    void Listen(boost::asio::ip::tcp::endpoint ep) {
        acc_.bind(ep);
        acc_.listen();  // default backlog
    }

    void StartAccept();
    
    // 强制下线逻辑
    bool Kick(const std::string& uuid);

    
private:

    bool RemoveSession(const std::string& uuid);

    bool running{true};
    // 通过sessions_储存这些会话的智能指针，保持其不被销毁
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
    boost::asio::io_context& context_;
    boost::asio::ip::tcp::acceptor acc_;
};

#endif