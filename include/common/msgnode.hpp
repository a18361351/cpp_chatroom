#ifndef MSGNODE_HEADER
#define MSGNODE_HEADER

#include <cassert>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <stdint.h>

const int TAG_LEN = 4;
const int LENGTH_LEN = 4;
const int HEAD_LEN = TAG_LEN + LENGTH_LEN;
const uint32_t MAX_CTX_LEN = 1024 * 1024;   // 消息长度上限，实际应用应该不会发送如此大的消息

// 消息种类（Tag）
enum {
    UNKNOWN_MSG = 0,
    TEXT_MSG,
    HELLO_MSG,
    ECHO_MSG,
    PING_MSG,
    PONG_MSG
};

// TODO(user): MsgNode可能需要分成RecvNode和SendNode？MsgNode同时用在发送和接收上语义有点不明

// 会话层用来存储数据的MsgNode节点类
// 在其中定义一个TLV协议格式：
//  | Tag(4字节) | Len(4字节) | Content(Len字节) |
class MsgNode {
    friend class Session;   // TODO(user): 把这些友元删掉，没有意义
    friend class ClientSession;
    friend class AsyncClientSess;
public:
    // Ctors

    // @brief 通过最大长度构造空节点对象
    // @param max_len 该节点分配缓冲区的内存大小，注意这是包括头部的，一般使用时要求max_len > HEAD_LEN，但这里不做检查
    // @ 构造的对象中，整个缓冲区长度应为max_len, 当前指针和内容长度均为0
    MsgNode(uint32_t max_len) : 
        data_(max_len > 0 ? new char[max_len] : nullptr), 
        cur_pos_(0), 
        ctx_len_(0),    // 我们实际上不知道内容长度是多少，设为0
        max_len_(max_len) {}

    // @brief 通过字符串和最大长度构造带有消息的节点对象，函数內部会进行消息的拷贝，同时根据msg_len和tag的值设定头部
    // @param content   要发送的消息的字符串
    // @param msg_len   字符串的长度，节点会根据这个长度来分配内存（HEAD_LEN + msg_len），这会写入消息格式的头部以及成员变量中
    // @param tag       消息的标记，这会写入消息格式的头部
    MsgNode(const char* content, uint32_t msg_len, uint32_t tag = 0) : 
        data_(msg_len > 0 ? new char[HEAD_LEN + msg_len] : nullptr), 
        cur_pos_(0),   // 当前指针指向起点
        ctx_len_(msg_len),
        max_len_(HEAD_LEN + msg_len) {
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

    // dtor
    ~MsgNode() {
        if (data_)
            delete[] data_;
    }

    // @brief 销毁缓冲区
    // @ 销毁缓冲区中内容，释放内存
    void Clear() {
        if (data_)
            delete[] data_;
        data_ = nullptr;
        cur_pos_ = 0;
        ctx_len_ = 0;
        max_len_ = 0;
    }

    // @brief 清空缓冲区
    // @ 将缓冲区内容设置为空，并把当前指针位置和内容长度清零
    void Zero() {
        cur_pos_ = 0;
        ctx_len_ = 0;
        // memset(data_, 0, max_len_); // 极端性能要求场景中，我们不需要真的去清零缓冲区
    }

    // @brief 扩张缓冲区（应该叫Expand更好？），new_len参数为缓冲区本身长度（调用时应传入ctx_len+HEAD_LEN）
    // @param new_len 新的缓冲区目标长度，该长度包括了格式中的头部
    // @ 如果原先的缓冲区大小 >= 新的缓冲区大小，该函数什么都不做
    // @ 否则会新分配一段内存，并把原先的内容全部拷贝至新内存
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

    // @Copy assign && Copy ctor
    // @因为项目中我们使用的是shared_ptr<MsgNode>，理论上不会涉及到MsgNode的拷贝
    // @但是为了这个类的一致性，我们还是把他实现在这里，同时加上警告，避免意外调用
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

    // 消息格式头部处理的函数
    
    uint32_t GetContentLen() const {
        return ctx_len_;
    }

    // @brief 从接收到的消息头部中的内容长度字段获取消息内容的长度，保存至content_len并返回
    // @return 消息内容的长度，转换为主机字节序
    // @warning 该函数仅在已经完整接收到消息头部的情况下才能调用！即已接收的数据 >= HEAD_LEN 
    uint32_t UpdateContentLenField() {
        memcpy(&ctx_len_, data_ + TAG_LEN, LENGTH_LEN);
        ctx_len_ = ntohl(ctx_len_);
        return ctx_len_;
    }

    // @brief 设置消息头部中的内容长度字段和content_len成员
    // @param 要设置的字段值，主机字节序
    // @warning 该函数仅设置消息头部中的内容长度字段，以及成员变量！不会对实际缓冲区长度有任何影响！
    void SetContentLenField(uint32_t content_len) {
        uint32_t net_cont_len = htonl(content_len);
        memcpy(data_ + TAG_LEN, &net_cont_len, LENGTH_LEN);
        ctx_len_ = content_len;
    }

    // @brief 获取消息头部中的Tag字段
    // @return 消息的tag，转换为主机字节序
    // @warning 该函数仅在已经完整接收到消息头部的情况下才能调用！即已接收的数据 >= HEAD_LEN
    uint32_t GetTagField() const {
        uint32_t tag;
        memcpy(&tag, data_, TAG_LEN);
        tag = ntohl(tag);
        return tag;
    }

    // @brief 设置消息头部中的Tag字段
    // @param tag 要设置的tag值，主机字节序
    void SetTagField(uint32_t tag) {
        uint32_t net_tag = htonl(tag);
        memcpy(data_ + TAG_LEN, &net_tag, LENGTH_LEN);
    }

    char* GetContent() {
        return data_ + HEAD_LEN;
    }

    const char* GetContent() const {
        return data_ + HEAD_LEN;
    }

    char* GetData() {
        return data_;
    }

// private:
public:
    char* data_;    // 包括tag, content_len, data的缓冲区，注意缓冲区中头部两变量为网络端字节序
    uint32_t cur_pos_;  // 当前读取/写入的位置（包括头部在内），用于读取（表示当前读取的位置）或写入（表示当前写入的位置）
    uint32_t ctx_len_;  // 内容的长度（不包括头部），注：这部分实际上也存储在格式头部中，但那部分是网络字节序的
    uint32_t max_len_;  // 当前缓冲区的长度，这是分配的整个内存块的实际长度（包括格式中的头部）
};


#endif