#ifndef CORO_SESSION_HEADER
#define CORO_SESSION_HEADER

// TODO(user): coro_session和session类不兼容，但session类和其他类耦合了，可能需要修改

#include <memory>
#include <deque>
#include <mutex>
#include <stdexcept>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "common/msgnode.hpp"

constexpr size_t INITIAL_NODE_SIZE = 1024;

class Server;   // 需要Server这个类型

using errcode = boost::system::error_code;

class CoroSession : public std::enable_shared_from_this<CoroSession> {
public:
    friend class Server;
    // 添加对strand的支持
    CoroSession(boost::asio::io_context& ctx, Server* srv) : 
        recv_ptr_(std::make_shared<MsgNode>(INITIAL_NODE_SIZE)), 
        ctx_(ctx), 
        sock_(ctx), 
        srv_(srv), 
        strand_(ctx.get_executor()) // strand保护在多个线程同时执行回调的情况下，不会并发调用回调
        {
        // 创建UUID
        boost::uuids::uuid u = boost::uuids::random_generator()();
        uuid_ = boost::uuids::to_string(u);
    }
    const std::string& uuid() const {
        return uuid_;
    }


    // 启动协程的执行，协程能在异步执行的同时保持同步的写法
    void Start() {
        auto coro = [self = shared_from_this()]() -> boost::asio::awaitable<void> {
            while (true) {
                // 不断运行的协程
                self->recv_ptr_->Zero();
                // 等待头部读入
                try {
                    auto ret = co_await boost::asio::async_read(self->sock_,
                        boost::asio::buffer(self->recv_ptr_->data_, HEAD_LEN),
                        boost::asio::use_awaitable);
        
                    // 协程版本的async操作会返回操作的字节数，错误时会直接抛出异常
                    // async_read的awaitable版本会返回操作的字节数（应该等于指定的参数）
                    // 连接被关闭时会提前返回（小于指定读取字节数）
                    // 出错时会抛异常boost::system::system_error
                    self->recv_ptr_->cur_pos_ += ret;
                    if (self->recv_ptr_->cur_pos_ >= HEAD_LEN) {
                        uint32_t content_len = self->recv_ptr_->UpdateContentLenField();
                        if (content_len > MAX_CTX_LEN) {
                            // 超出最大报文长度了！
                            self->sock_.close();
                            co_return;
                        }
                        if (content_len + HEAD_LEN > self->recv_ptr_->max_len_) {
                            self->recv_ptr_->Reallocate(content_len + HEAD_LEN);
                        }
                    } else {
                        // 头部没读完就关闭了连接，这个部分头部数据没有意义了
                        self->sock_.close();
                        co_return;
                    }
                } catch (const boost::system::system_error& e) { // 头部读入错误
                    fprintf(stderr, "Error reading header: %s\n", e.what());
                    self->sock_.close();
                    co_return; // 结束协程
                }
                // 数据体读入
                try {
                    uint32_t content_len = self->recv_ptr_->GetContentLen();
                    auto ret = co_await boost::asio::async_read(self->sock_,
                        boost::asio::buffer(self->recv_ptr_->data_ + HEAD_LEN, content_len),
                        boost::asio::use_awaitable);
        
                    self->recv_ptr_->cur_pos_ += ret;
                    if (self->recv_ptr_->cur_pos_ >= HEAD_LEN + content_len) {
                        uint32_t tag = self->recv_ptr_->GetTagField();
                        self->ReceiveHandler(content_len, tag);
                        // done
                    } else {
                        // 接收了部分数据
                        self->sock_.close();
                        co_return;
                    }
                } catch (const boost::system::system_error& e) { // 内容部分读入错误
                    fprintf(stderr, "Error reading content: %s\n", e.what());
                    self->sock_.close();
                    co_return; // 结束协程
                }
            }
        };
        // 启动协程
        boost::asio::co_spawn(ctx_, coro, boost::asio::detached);
    }

    // -----------------------
    // 读取时的回调操作
public:
    // 该方法在整个消息被读取完整后被调用
    void ReceiveHandler(uint32_t content_len, uint32_t tag);

    std::shared_ptr<MsgNode> GetRecvNode() {
        return recv_ptr_;
    }

private:
    // MsgNode recv_buf_{1024};
    std::shared_ptr<MsgNode> recv_ptr_;
    
    // -----------------------

    // -----------------------
    // 异步发送时的队列操作
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
    std::deque<msg_ptr> send_q_;    // 发送队列
    std::mutex send_latch_;         // 队列锁
    // -----------------------

private:
    bool down_{false};  // 该session被强制下线
    std::string uuid_;
    boost::asio::io_context& ctx_;
    boost::asio::ip::tcp::socket sock_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    
    Server* srv_;
};


#endif 