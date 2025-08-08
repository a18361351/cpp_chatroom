#ifndef CLIENT_CLASS_HEADER
#define CLIENT_CLASS_HEADER

#include <condition_variable>
#include <deque>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <common/msgnode.hpp>

// Synced
// 其登录和注册接口是同步的
namespace chatroom::client {
    class GatewayHelper {
        public:
        GatewayHelper(boost::asio::io_context& ctx) : ctx_(ctx) {}
        bool Login(const std::string& username, const std::string& passcode, std::string& token, std::string& server_addr, uint64_t& uid);
        bool Register(const std::string& username, const std::string& passcode);
        void SetRemoteAddress(const std::string& host, const std::string& port);
        private:
        boost::asio::io_context& ctx_;
        std::string host_;
        std::string port_;
    };
    
    // FIXME: strand，似乎异步操作同时读写sock会导致奇怪的问题
    class AsyncClient : public std::enable_shared_from_this<AsyncClient> {
    public:
        explicit AsyncClient(boost::asio::io_context& ctx) : sock_(ctx), ctx_(ctx), strand_(ctx_.get_executor()) {}
        // 启动连接
        void Start(const boost::asio::ip::tcp::endpoint& remote);
        void ConnectedHandler();
        void Receiver();
    
        void ReceiveHead();
    
        void ReceiveContent();
    
        // 通过字符串和最大长度构造带有消息的节点对象，函数內部会进行消息的拷贝，同时根据msg_len和tag的值设定头部
        void Send(const char* msg, uint32_t msg_len, uint32_t tag);
    
        void ReceiveHandler(uint32_t len, uint32_t tag);
    
        void Close();
    
        void WorkerFn();
    
        void Verify(uint64_t uid, const std::string& token);
    
        void WorkerJoin();
        bool Running() const;
    
        MsgNode recv_buf_{1024};
    private:
        boost::asio::ip::tcp::socket sock_;
        boost::asio::io_context& ctx_;
        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    
        // send queue
        std::deque<std::shared_ptr<MsgNode>> send_q_;
        std::shared_ptr<MsgNode> sending_;
        std::mutex mtx_;
        std::thread worker_;
        std::condition_variable cv_;
        std::atomic_bool running_;
    };
}

#endif