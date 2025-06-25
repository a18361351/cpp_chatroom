#ifndef SESSION_HEADER
#define SESSION_HEADER

#include <memory>
#include <deque>

#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
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
    // Ctors
    // 通过最大长度构造空节点对象
    MsgNode(std::size_t max_len) : data_(max_len > 0 ? new char[max_len] : nullptr), cur_pos_(0), max_len_(max_len) {}
    // 通过字符串和最大长度构造带有消息的节点对象，一次拷贝
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
        Receiver();
    }

    // -----------------------
    // 读取时的回调操作
public:
    void ReceiveHandler(std::size_t bytes_rcvd);

    void Receiver() {
        auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
            if (err) {
                // tell that error!
                return;
            }
            if (bytes_rcvd == 0) {
                // closing
                return;
            }
            self->ReceiveHandler(bytes_rcvd);
            self->Receiver();   // 不断接收信息
        };
        sock_.async_receive(boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.max_len_ - recv_buf_.cur_pos_), cb);
        // boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.max_len_ - recv_buf_.cur_pos_), cb);
    }

private:
    MsgNode recv_buf_{1024};

    // -----------------------

    // -----------------------
    // 异步发送时的队列操作
public:
    using msg_ptr = std::shared_ptr<MsgNode>;
    using locker = std::unique_lock<std::mutex>;
    void Send(const char* msg, int send_len);
    // 获取
    // 只要还有数据要发送，QueueSend就会一直被调用
    void QueueSend();
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