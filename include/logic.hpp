#ifndef LOGIC_HEADER
#define LOGIC_HEADER

#include <condition_variable>
#include <queue>
#include <cstdint>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <deque>
#include <memory>

#include "session.hpp"
#include "util_class.hpp"

enum {
    UNKNOWN_MSG = 0,
    TEXT_MSG,
    HELLO_MSG,
    ECHO_MSG,
    PING_MSG,
    PONG_MSG
};

using CbSessType = std::shared_ptr<Session>;
using RcvdMsgType = std::shared_ptr<MsgNode>; // ReceiveMsg

// 对于特定tag消息的回调函数，传入参数为Session和MsgNode
using FuncCallback = 
    std::function<void(CbSessType, RcvdMsgType)>;

// 逻辑节点：Session发送来的msg
class LogicNode {
    friend class LogicSys;
public:
    LogicNode();
    // LogicNode(CbSessType sess) : sess_(std::move(sess)), msg_(sess_->GetRecvNode()) {}

    LogicNode(CbSessType sess, RcvdMsgType msg) 
        : sess_(std::move(sess)), msg_(msg) {}
private:
    CbSessType sess_; // LogicNode包含了Session的智能指针，防止其被释放
    RcvdMsgType msg_;                  // 接收的MsgNode的指针
    // std::shared_ptr<MsgNode> msg_;
};

// 逻辑系统：处理消息，并根据它们的种类调用相应的处理函数
class LogicSys {
    friend class Singleton<LogicSys>;
public:
    ~LogicSys() {
        stop_ = true;
        cv_.notify_all();
        worker_.join(); // 
    }
    void PostMsgToQueue(std::shared_ptr<LogicNode> msg) {
        std::unique_lock<std::mutex> lck(mutex_);
        msg_que_.push(std::move(msg));
        if (msg_que_.size() == 1) {
            // 由0变1表示新增了一个对象，需要唤醒
            cv_.notify_one();
        }
    }

private:
    LogicSys() : stop_(false) {
        RegisterCallbacks();
        StartWorker();
    }

    // 工作线程的处理函数DealMsg逻辑
    void DealMsg() {
        for (;;) {
            std::unique_lock<std::mutex> lck(mutex_);
            while (msg_que_.empty() && !stop_) {
                cv_.wait(lck);
            }

            if (stop_) {
                while (!msg_que_.empty()) {
                    std::shared_ptr<LogicNode> msg_node = std::move(msg_que_.front());
                    msg_que_.pop();
                    auto cb_iter = cbs_.find(msg_node->msg_->GetTagField());
                    if (cb_iter == cbs_.end()) {
                        continue;
                    }
                    cb_iter->second(msg_node->sess_, msg_node->msg_);
                }
                break;
            }

            std::shared_ptr<LogicNode> msg_node = msg_que_.front();
            msg_que_.pop();
            // lck这时候已经可以释放了！
            lck.unlock();

            auto cb_iter = cbs_.find(msg_node->msg_->GetTagField());
            if (cb_iter == cbs_.end()) {
                continue;
            }
            cb_iter->second(msg_node->sess_, msg_node->msg_);
        }
    }
    
    // worker线程启动
    void StartWorker() {
        worker_ = std::thread(&LogicSys::DealMsg, this);
    }

    // 创建回调
    void RegisterCallbacks() {
        using namespace std::placeholders;
        cbs_[HELLO_MSG] = std::bind(&LogicSys::WelcomeMsgCallback, this, _1, _2);
        cbs_[TEXT_MSG] = std::bind(&LogicSys::TextMsgCallback, this, _1, _2);
        cbs_[ECHO_MSG] = std::bind(&LogicSys::EchoMsgCallback, this, _1, _2);
        // TODO(user): 可以添加更多的回调
    }

    void WelcomeMsgCallback(CbSessType sess, RcvdMsgType msg) {
        sess->Send("Welcome to the server!", 0);
    }
    void TextMsgCallback(CbSessType sess, RcvdMsgType msg) {
        fwrite(msg->GetContent(), 1, msg->GetContentLen(), stdout);
        fflush(stdout);
    }
    void EchoMsgCallback(CbSessType sess, RcvdMsgType msg) {
        // 回显消息
        sess->Send(msg->GetContent(), msg->GetContentLen(), ECHO_MSG);
    }



    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_; // TODO(user): 可能atomic会更好？
    std::queue<std::shared_ptr<LogicNode>> msg_que_;
    std::unordered_map<uint32_t, FuncCallback> cbs_;
    

};


#endif 