#ifndef SESSION_HEADER
#define SESSION_HEADER

#include <memory>
#include <deque>
#include <mutex>
#include <stdexcept>

#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "common/msgnode.hpp"

constexpr size_t INITIAL_NODE_SIZE = 1024;

class Server;   // 需要Server这个类型

using errcode = boost::system::error_code;

class Session : public std::enable_shared_from_this<Session> {
public:
    friend class Server;
    Session(boost::asio::ip::tcp::socket&& sock, Server* srv) : sock_(std::move(sock)), srv_(srv), recv_ptr_(std::make_shared<MsgNode>(INITIAL_NODE_SIZE)) {
        // 创建UUID
        boost::uuids::uuid u = boost::uuids::random_generator()();
        uuid_ = boost::uuids::to_string(u);
    }
    const std::string& uuid() const {
        return uuid_;
    }


    // 已建立链接的Sess开始执行
    // 注意Session的生命周期管理由自己以及Server的sessions集合对象管理
    void Start() {
        Receiver();
    }

    // -----------------------
    // 读取时的回调操作
public:
    // 该方法在整个消息被读取完整后被调用
    void ReceiveHandler(uint32_t content_len, uint32_t tag);

    void Receiver() {
        if (recv_ptr_->cur_pos_ < HEAD_LEN) {
            ReceiveHead();
        } else {
            ReceiveContent();
        }
    }

    void ReceiveHead() {
        auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
            if (err) {
                // tell that error!
                fprintf(stdout, "Receiver() received an error! %s\n", err.what().c_str());
                fflush(stdout);
                return;
            }
            fprintf(stdout, "Receiver() received %lu bytes\n", bytes_rcvd);
            fflush(stdout);
            // cur_pos更新
            self->recv_ptr_->cur_pos_ += bytes_rcvd;
            if (self->recv_ptr_->cur_pos_ >= HEAD_LEN) {
                // 处理了报文长度部分
                // 此时我们可以将ctx_len部分来获取出来了

                uint32_t content_len = self->recv_ptr_->UpdateContentLenField();

                if (content_len > MAX_CTX_LEN) {
                    // 超出最大报文长度了！
                    self->sock_.close();
                    return;
                }
                if (content_len + HEAD_LEN > self->recv_ptr_->max_len_) {
                    self->recv_ptr_->Reallocate(content_len + HEAD_LEN);
                }
                self->ReceiveContent();
            } else {
                // 继续接收头部
                self->ReceiveHead();
            }
        };
    
        // 消息头部
        sock_.async_receive(boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, HEAD_LEN - recv_ptr_->cur_pos_), cb);
        // boost::asio::async_read(sock_, boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, HEAD_LEN - recv_ptr_->cur_pos_), cb);
    }

    void ReceiveContent() {
        auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
            if (err) {
                // tell that error!
                fprintf(stdout, "Receiver() received an error! %s\n", err.what().c_str());
                fflush(stdout);
                return;
            }
            fprintf(stdout, "Receiver() received %lu bytes\n", bytes_rcvd);
            fflush(stdout);
            // cur_pos更新
            self->recv_ptr_->cur_pos_ += bytes_rcvd;

            if (self->recv_ptr_->cur_pos_ >= self->recv_ptr_->ctx_len_ + HEAD_LEN) {
                // 处理了一整个消息
                uint32_t tag = self->recv_ptr_->GetTagField();
                self->ReceiveHandler(self->recv_ptr_->ctx_len_, tag);
                // 处理完毕后清除缓冲区，准备下一次接收
                self->recv_ptr_->Zero();
                self->ReceiveHead();
            } else {
                self->ReceiveContent();
            }
        };
        // 消息体
        sock_.async_receive(boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, recv_ptr_->ctx_len_ + HEAD_LEN - recv_ptr_->cur_pos_), cb);
        // boost::asio::async_read(sock_, boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, recv_ptr_->ctx_len_ + HEAD_LEN - recv_ptr_->cur_pos_), cb);
    }

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
    boost::asio::ip::tcp::socket sock_;
    Server* srv_;
};

#endif