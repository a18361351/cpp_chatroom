#ifndef SERVER_SESSION_HEADER
#define SERVER_SESSION_HEADER

// 后台服务器管理的与客户端连接的会话（Session）类
// Session对象通过shared_from_this维护自己的生命周期，同时持有SessionManager指针

#include <memory>
#include <deque>
#include <mutex>

// #include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include "common/msgnode.hpp"

namespace chatroom::backend {
    constexpr size_t INITIAL_NODE_SIZE = 1024;
    // 定义UID的类型，引用类型和const引用类型；uint64_t这种简单类型可以不取引用
    using UID = uint64_t;   // std::string
    using UIDRef = uint64_t&;    // std::string&

    class SessionManager;
    class MsgHandler;
    class Session : public std::enable_shared_from_this<Session> {
        friend class SessionManager;
        public:
        // 添加对strand的支持
        Session(boost::asio::io_context& ctx) : 
            recv_ptr_(std::make_shared<MsgNode>(INITIAL_NODE_SIZE)), 
            sock_(ctx), 
            strand_(ctx.get_executor()) // strand保护在多个线程同时执行回调的情况下，不会并发调用回调
            {}

        Session(boost::asio::io_context& ctx, 
                boost::asio::ip::tcp::socket&& socket, 
                uint64_t user_id,
                std::weak_ptr<MsgHandler> handler,
                std::weak_ptr<SessionManager> mgr
            ) : 
            user_id_(user_id)
            , recv_ptr_(std::make_shared<MsgNode>(INITIAL_NODE_SIZE))
            , sock_(std::move(socket))
            , strand_(ctx.get_executor())
            , handler_(std::move(handler))
            , mgr_(std::move(mgr))
        {

        }
        uint64_t GetUserId() const {
            return user_id_;
        }
    
    
        // 已建立链接的Sess开始执行
        // 注意Session的生命周期管理由自己以及Server的sessions集合对象管理
        void Start() {
            Receiver();
        }
    
        // 该方法在整个消息被读取完整后被调用
        void ReceiveHandler(uint32_t content_len, uint32_t tag);
    
        void Receiver() {
            if (recv_ptr_->cur_pos_ < HEAD_LEN) {
                ReceiveHead();
            } else {
                ReceiveContent();
            }
        }
    
        void ReceiveHead();
    
        void ReceiveContent();
    
        std::shared_ptr<MsgNode> GetRecvNode() {
            return recv_ptr_;
        }
    
    public:
        using msg_ptr = std::shared_ptr<MsgNode>;
        using locker = std::unique_lock<std::mutex>;
    
        // Session对外的发送接口1，函数会将消息放入队列中等待发送
        void Send(const char* content, uint32_t send_len, uint32_t tag);
    
        // Session对外的发送接口2，函数会将消息放入队列中等待发送
        void Send(const std::string& msg, uint32_t tag) {
            Send(msg.c_str(), msg.size(), tag);
        }
    
        // Session对外的发送接口3，函数会将已有的消息放入队列中等待发送
        void Send(std::shared_ptr<MsgNode> msg);
    
        // 获取
        // 只要还有数据要发送，QueueSend就会一直被调用
        void QueueSend();
    private:
        // TODO(user): 发送队列可以设置长度限制，以保证不会产生发送速率过快的情况
        // -----------------------

        // ***** 用户相关 *****
        UID user_id_;  // 这里sess_id == user_id
        // ***** 接收操作 *****
        std::shared_ptr<MsgNode> recv_ptr_;
        // ***** 发送操作 *****
        std::deque<msg_ptr> send_q_;    // 发送队列
        std::mutex send_latch_;         // 队列锁
        // 连接相关
        bool down_{false};  // 该标志被设为true后，session不会有下一步的动作
        boost::asio::ip::tcp::socket sock_;
        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
        // ***** 外部对象管理 *****
        std::weak_ptr<MsgHandler> handler_;
        std::weak_ptr<SessionManager> mgr_;
    };
}


#endif