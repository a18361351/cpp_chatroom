#ifndef SESSION_HEADER
#define SESSION_HEADER

#include <memory>
#include <deque>

#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <mutex>

class Server;

// const int BUF_SIZE = 1024;

using errcode = boost::system::error_code;

// 用来存储数据的MsgNode节点类
//  TODO(future): 也许未来可以避免频繁的内存分配和释放？
class MsgNode {
    friend class Session;
public:
    MsgNode(const char* msg, std::size_t msg_len) : data_(msg_len > 0 ? new char[msg_len] : nullptr), cur_pos_(0), max_len_(msg_len) {
        if (msg_len > 0)
            memcpy(data_, msg, msg_len);
    }
    ~MsgNode() {
        if (data_)
            delete[] data_;
    }
private:
    char* data_;
    std::size_t cur_pos_;
    std::size_t max_len_;
};


class Session : public std::enable_shared_from_this<Session> {
public:
    friend class Server;
    Session(boost::asio::ip::tcp::socket sock, Server* srv) : sock_(std::move(sock)), srv_(srv) {
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
        DoRecv();
    }

    void DoRecv();
    // void DoSend();

    // -----------------------
    // 异步发送时的队列操作
    using msg_ptr = std::shared_ptr<MsgNode>;
    using locker = std::unique_lock<std::mutex>;
    void Send(const char* msg, int send_len) {
        msg_ptr ptr = std::make_shared<MsgNode>(msg, send_len);
        bool start_send;
        {
            locker lck(send_latch_);
            start_send = send_q_.empty();
            send_q_.push_back(ptr);
        }
        if (start_send) {
            QueueSend();
        }
    }
    // 只要还有数据要发送，QueueSend就会一直被调用
    void QueueSend() {
        send_latch_.lock();
        if (!send_q_.empty()) {
            msg_ptr ptr = send_q_.front();
            send_latch_.unlock();
            auto self = shared_from_this();
            boost::asio::async_write(sock_, boost::asio::buffer(ptr->data_ + ptr->cur_pos_, ptr->max_len_ - ptr->cur_pos_),
                [self = std::move(self)](const errcode& err, size_t bytes_sent) {
                    if (err) {
                        // tell that error!
                        self->srv_->RemoveSession(self->uuid_);
                        self->sock_.close();
                        fprintf(stderr, "Error in sending: %s\n", err.what().c_str());
                        locker lck(self->send_latch_);
                        self->send_q_.clear();
                        return;
                    }
                    {
                        locker lck(self->send_latch_);
                        msg_ptr ptr = self->send_q_.front();
                        ptr->cur_pos_ += bytes_sent;
                        if (ptr->cur_pos_ >= ptr->max_len_) {
                            self->send_q_.pop_front();
                        }
                    }
                    if (!self->send_q_.empty()) {
                        self->QueueSend();
                    }
                });
        } else {
            send_latch_.unlock();
        }
         
        // locker lck(send_latch_);
        // if (!send_q_.empty()) {
        //     auto self = shared_from_this();
        //     msg_ptr ptr = send_q_.front();
        //     boost::asio::async_write(sock_, boost::asio::buffer(ptr->data_ + ptr->cur_pos_, ptr->max_len_ - ptr->cur_pos_),
        //         [self = std::move(self)](const errcode& err, size_t bytes_sent) {
        //             if (err) {
        //                 // tell that error!
        //                 self->srv_->RemoveSession(self->uuid_);
        //                 self->sock_.close();
        //                 fprintf(stderr, "Error in sending: %s\n", err.what().c_str());
        //                 locker lck(self->send_latch_);
        //                 self->send_q_.clear();
        //                 return;
        //             }
        //             {
        //                 locker lck(self->send_latch_);
        //                 msg_ptr ptr = self->send_q_.front();
        //                 ptr->cur_pos_ += bytes_sent;
        //                 if (ptr->cur_pos_ >= ptr->max_len_) {
        //                     self->send_q_.pop_front();
        //                 }
        //             }
        //             if (!self->send_q_.empty()) {
        //                 self->QueueSend();
        //             }
        //         });
        // }
    }
private:
    std::deque<msg_ptr> send_q_;    // 发送队列
    std::mutex send_latch_;         // 队列锁
    // -----------------------

private:
    bool down_{false};  // 该session被强制下线
    std::string uuid_;
    boost::asio::ip::tcp::socket sock_;
    Server* srv_;
    // std::vector<char> buf_;
};

#endif