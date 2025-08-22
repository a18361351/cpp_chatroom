#ifndef MSGNODE_HEADER
#define MSGNODE_HEADER

// msgnode.hpp: 定义了整个项目中用于网络传输的TLV报文包格式
// **********************************************
// 当我们使用这个报文包时，其一般内容为：
//  | <----- 4B ------->|<------ 4B ------->|<--- len size --->| (总共content_len + 8B)
//  | Tag(4字节，网络序) | Len(4字节，网络序) | Content(Len字节) |
//
//  其中Tag标记了这个报文的类型；Len指定了后续内容（Content）的长度，因此我们可以确定这之后的Len字节均属于当前的报文。
// 而Len字节之后，如果还有内容的话，那么接下来8B属于下一个报文的头部，我们又可以从中得到下一个报文的Tag和Len信息，以及
// 后续的Content长度，这样我们清晰地分割了报文，解决了粘包的问题。
//
//  因此，服务端在接收任何MsgNode报文时，采用的逻辑均为：接收8B头部 -> 获取Len -> 接收Len长度内容->
//  循环。因此，发送者需要严格遵守该报文的协议规则。
//
//  值得注意的是，报文的原始内容中Tag字段和Len字段一定是网络序的。因此在读取时要进行ntoh，写入时进行hton的转换！

#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "utils/field_op.hpp"

namespace chatroom {
enum TagType {
    DEBUG = 0,       // 用于调试的消息格式，收到后把消息显示在日志上
    VERIFY,          // JSON格式的身份验证消息
    VERIFY_DONE,     // 完成验证的消息
    CHAT_MSG,        // 格式：[uint64_t: 目标id][消息内容]
    CHAT_MSG_TOCLI,  // 发送给客户端的聊天消息，格式：[uint64_t：发送者id][消息内容]
    GROUP_CHAT_MSG,  // 格式：[uint64_t：目标组的group_id][消息内容]
    PING,            // 心跳包
    RESERVED
};

// @brief 返回TagType的字符串表示
inline std::string_view TagTypeStr(enum TagType e) {
    switch (e) {
        case DEBUG:
            return "DEBUG";
        case VERIFY:
            return "VERIFY";
        case VERIFY_DONE:
            return "VERIFY_DONE";
        case CHAT_MSG:
            return "CHAT_MSG";
        case CHAT_MSG_TOCLI:
            return "CHAT_MSG_TOCLI";
        case GROUP_CHAT_MSG:
            return "GROUP_CHAT_MSG";
        case RESERVED:
            return "RESERVED";
        default:
            return "UNKNOWN";
    }
}

const int TAG_LEN = 4;
const int LENGTH_LEN = 4;
const int HEAD_LEN = TAG_LEN + LENGTH_LEN;
const uint32_t MAX_CTX_LEN = 1024 * 1024;  // 消息长度上限，实际应用应该不会发送如此大的消息

// 会话层用来存储数据的MsgNode节点类
//  | Tag(4字节，网络序) | Len(4字节，网络序) | Content(Len字节) |
class MsgNode {
   public:
    // Ctors

    // @brief 通过最大长度构造空节点对象
    // @param max_len 该节点分配缓冲区的内存大小，注意这是包括头部的，一般使用时要求max_len > HEAD_LEN，但这里不做检查
    // @ 构造的对象中，整个缓冲区长度应为max_len, 当前指针和内容长度均为0
    explicit MsgNode(uint32_t max_len)
        : data_(max_len > 0 ? new char[max_len] : nullptr),
          cur_pos_(0),
          ctx_len_(0),  // 我们实际上不知道内容长度是多少，设为0
          max_len_(max_len) {}

    // @brief 通过字符串和最大长度构造带有消息的节点对象，函数內部会进行消息的拷贝，同时根据msg_len和tag的值设定头部
    // @param content   要发送的消息的字符串
    // @param msg_len   字符串的长度，节点会根据这个长度来分配内存（HEAD_LEN +
    // msg_len），这会写入消息格式的头部以及成员变量中
    // @param tag       消息的标记，这会写入消息格式的头部
    MsgNode(const char *content, uint32_t msg_len, uint32_t tag = 0)
        : data_(msg_len > 0 ? new char[HEAD_LEN + msg_len] : nullptr),
          cur_pos_(0),  // 当前指针指向起点
          ctx_len_(msg_len),
          max_len_(HEAD_LEN + msg_len) {
        assert(TAG_LEN == sizeof(uint32_t));
        assert(LENGTH_LEN == sizeof(uint32_t));
        if (msg_len > 0) {
            // uint32_t net_msg_len = htonl(msg_len);
            // uint32_t net_tag = htonl(tag);
            // memcpy(data_, &net_tag, TAG_LEN);
            // memcpy(data_ + TAG_LEN, &net_msg_len, LENGTH_LEN);
            WriteNetField32(data_, tag);
            WriteNetField32(data_ + TAG_LEN, msg_len);
            memcpy(data_ + TAG_LEN + LENGTH_LEN, content, msg_len);
        } else {
            throw std::invalid_argument("Attempt to create a zero-sized message");
        }
    }

    // @brief 直接通过原始指针来构造指向该缓冲区的对象，很不安全
    // @param data_ptr 指向已有缓冲区的指针，该对象会在MsgNode析构时被自动delete
    // @param data_len 该缓冲区的长度
    [[deprecated("Initializing MsgNode by raw pointer is unsafe and may cause problems if misused.")]] MsgNode(
        char *data_ptr, uint32_t data_len)
        : data_(data_ptr), cur_pos_(0), ctx_len_(0), max_len_(data_len) {}

    // dtor
    ~MsgNode() {
        if (data_) delete[] data_;
    }

    // @brief 销毁缓冲区，清除缓冲区中内容，释放内存
    void Clear() {
        if (data_) delete[] data_;
        data_ = nullptr;
        cur_pos_ = 0;
        ctx_len_ = 0;
        max_len_ = 0;
    }

    // @brief 重新设定当前读写指针，不会对实际数据造成任何影响
    // @ 可用于缓冲区内容的复用，避免拷贝
    void ResetCurPos() { cur_pos_ = 0; }

    // @brief 将缓冲区内容设置为空，并把当前指针位置和内容长度清零
    void Zero() {
        cur_pos_ = 0;
        ctx_len_ = 0;
        // memset(data_, 0, max_len_); // 性能要求场景中，我们不需要真的去清零缓冲区
    }

    // @brief 扩张缓冲区，new_len参数为缓冲区本身长度（调用时应传入ctx_len+HEAD_LEN）
    // @param new_len 新的缓冲区目标长度，该长度包括了格式中的头部
    // @ 如果原先的缓冲区大小 >= 新的缓冲区大小，该函数什么都不做
    // @ 否则会新分配一段内存，并把原先的内容全部拷贝至新内存
    void Reallocate(uint32_t new_len) {
        // new/delete实现中没有realloc()类似的函数，只能删了重新分配
        // 扩展空间的情况
        if (data_ && new_len > max_len_) {
            char *tmp = new char[new_len];
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
    [[deprecated("Please be wary that you are copying a MsgNode object.")]] MsgNode &operator=(const MsgNode &rhs) {
        // 自我赋值检查
        if (this == &rhs) {
            return *this;
        }

        if (!data_ || rhs.max_len_ > max_len_) {
            if (data_) {
                delete[] data_;
            }
            data_ = new char[rhs.max_len_];
        }
        memcpy(data_, rhs.data_, rhs.max_len_);
        max_len_ = std::max(max_len_, rhs.max_len_);
        cur_pos_ = rhs.cur_pos_;
        ctx_len_ = rhs.ctx_len_;
        return *this;
    }
    [[deprecated("Please be wary that you are copying a MsgNode object.")]] MsgNode(const MsgNode &rhs) {
        data_ = new char[rhs.max_len_];

        memcpy(data_, rhs.data_, rhs.max_len_);
        max_len_ = rhs.max_len_;
        cur_pos_ = rhs.cur_pos_;
        ctx_len_ = rhs.ctx_len_;
    }

    // 消息格式头部处理的函数

    // @brief 返回内容的长度，需要事先调用UpdateContentLenField()
    // @ return 消息内容的长度，转换为主机序字节
    uint32_t GetContentLen() const { return ctx_len_; }

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
        // uint32_t net_cont_len = htonl(content_len);
        // memcpy(data_ + TAG_LEN, &net_cont_len, LENGTH_LEN);
        WriteNetField32(data_ + TAG_LEN, content_len);
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
        // uint32_t net_tag = htonl(tag);
        // memcpy(data_ + TAG_LEN, &net_tag, LENGTH_LEN);
        WriteNetField32(data_, tag);
    }

    char *GetContent() { return data_ + HEAD_LEN; }

    const char *GetContent() const { return data_ + HEAD_LEN; }

   public:
    char *data_;  // 包括tag, content_len, data的缓冲区，注意缓冲区中头部两变量为网络端字节序
    uint32_t
        cur_pos_;  // 当前读取/写入的位置（包括头部在内），用于读取（表示当前读取的位置）或写入（表示当前写入的位置）
    uint32_t ctx_len_;  // 内容的长度（不包括头部），注：这部分实际上也存储在格式头部中，但那部分是网络字节序的
    uint32_t max_len_;  // 当前缓冲区的长度，这是分配的整个内存块的实际长度（包括格式中的头部）
};
}  // namespace chatroom

#endif