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

class Server;

// const int BUF_SIZE = 1024;

using errcode = boost::system::error_code;

const int TAG_LEN = 4;
const int LENGTH_LEN = 4;
const int HEAD_LEN = TAG_LEN + LENGTH_LEN;
const uint32_t MAX_CTX_LEN = 1024 * 1024;   // 消息长度上限，实际应用应该不会发送如此大的消息

// 会话层用来存储数据的MsgNode节点类
// 在其中定义一个TLV协议格式：
//  | Tag(4字节) | Len(4字节) | Content(Len字节) |
class MsgNode {
    friend class Session;
public:
    // Ctors
    // 通过最大长度构造空节点对象
    MsgNode(uint32_t max_len, uint32_t tag = 0) : 
        data_(max_len > 0 ? new char[TAG_LEN + LENGTH_LEN + max_len] : nullptr), 
        cur_pos_(0), 
        ctx_len_(max_len),
        max_len_(max_len) {
        assert(TAG_LEN == sizeof(uint32_t));
        assert(LENGTH_LEN == sizeof(uint32_t));
        if (max_len > 0) {
            uint32_t net_max_len = htonl(max_len);
            uint32_t net_tag = htonl(tag);
            memcpy(data_, &net_tag, TAG_LEN);
            memcpy(data_ + TAG_LEN, &net_max_len, LENGTH_LEN);
        } else {
            throw std::invalid_argument("Attempt to create a zero-sized message");
        }
    }
    // 通过字符串和最大长度构造带有消息的节点对象，一次拷贝
    MsgNode(const char* content, uint32_t msg_len, uint32_t tag = 0) : 
        data_(msg_len > 0 ? new char[TAG_LEN + LENGTH_LEN + msg_len] : nullptr), 
        cur_pos_(0), 
        ctx_len_(msg_len),
        max_len_(msg_len) {
        assert(TAG_LEN == sizeof(uint32_t));
        assert(LENGTH_LEN == sizeof(uint32_t));
        if (msg_len > 0) {
            uint32_t net_msg_len = htonl(msg_len);
            uint32_t net_tag = htonl(tag);
            memcpy(data_, &net_tag, TAG_LEN);
            memcpy(data_ + TAG_LEN, &net_msg_len, LENGTH_LEN);
            memcpy(data_ + TAG_LEN + LENGTH_LEN, content, msg_len);
        } else {
            throw std::invalid_argument("Attempt to create a zero-sized message");
        }
    }
    ~MsgNode() {
        if (data_)
            delete[] data_;
    }

    // 销毁缓冲区
    void Clear() {
        if (data_)
            delete[] data_;
        data_ = nullptr;
        cur_pos_ = 0;
        ctx_len_ = 0;
        max_len_ = 0;
    }

    // 清空缓冲区
    void Zero() {
        cur_pos_ = 0;
        ctx_len_ = 0;
        // memset(data_, 0, max_len_); // 极端性能要求场景中，我们不需要真的去清零缓冲区
    }

    // 扩张缓冲区（应该叫Expand更好？），new_len参数为缓冲区本身长度（调用时应传入ctx_len+HEAD_LEN）
    void Reallocate(uint32_t new_len) {
        // new/delete实现中没有realloc()类似的函数，只能删了重新分配
        // 扩展空间的情况
        if (data_ && new_len > max_len_) {
            char* tmp = new char[new_len];
            memcpy(tmp, data_, max_len_);
            delete[] data_;
            data_ = tmp;
        }
        // data_为空的情况
        if (!data_) {
            data_ = new char[new_len];
            cur_pos_ = 0;
            ctx_len_ = 0;
        }
        max_len_ = std::max(max_len_, new_len);
    }

    // Copy assign && Copy ctor
    // 因为项目中我们使用的是shared_ptr<MsgNode>，理论上不会涉及到MsgNode的拷贝
    // 但是为了这个类的一致性，我们还是把他实现在这里，同时加上警告，避免意外调用
    MsgNode& operator=(const MsgNode& rhs) {
        printf("WARNING: MsgNode's copy assign called!\n");
        if (!data_ || rhs.max_len_ > max_len_) {
            if (data_) {
                delete[] data_;
            }
            data_ = new char[rhs.max_len_];
        }
        memcpy(data_, rhs.data_,rhs.max_len_);
        max_len_ = std::max(max_len_, rhs.max_len_);
        cur_pos_ = rhs.cur_pos_;
        ctx_len_ = rhs.ctx_len_;
        return *this;
    }
    MsgNode(const MsgNode& rhs) {
        printf("WARNING: MsgNode's copy ctor called!\n");
        data_ = new char[rhs.max_len_];
        
        memcpy(data_, rhs.data_,rhs.max_len_);
        max_len_ = rhs.max_len_;
        cur_pos_ = rhs.cur_pos_;
        ctx_len_ = rhs.ctx_len_;
    }
    
private:
    char* data_;    // 包括tag, ctx_len, data的缓冲区，注意tag和ctx_len为网络端字节序
    uint32_t cur_pos_;  // 当前读取/写入的位置（包括头部在内）
    uint32_t ctx_len_;  // 内容的长度（不包括头部） TODO(user): content写成ctx了
    uint32_t max_len_;  // 当前缓冲区的最大长度
};



class Session : public std::enable_shared_from_this<Session> {
public:
    friend class Server;
    Session(boost::asio::ip::tcp::socket&& sock, Server* srv) : sock_(std::move(sock)), srv_(srv) {
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

    void Receiver();

private:
    MsgNode recv_buf_{1024};
    
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