#include "log/log_manager.hpp"
#include "server/msg_handler.hpp"
#include "utils/field_op.hpp"

bool chatroom::backend::MsgHandler::PostMessage(CbSessType sess, RcvdMsgType msg) {
    std::unique_lock lock(lck_);
    q_.emplace(std::move(sess), std::move(msg));
    if (q_.size() == 1) {
        lock.unlock();
        cv_.notify_one();
    }
    return true;
}

bool chatroom::backend::MsgHandler::Start() {
    std::unique_lock lock(lck_);
    if (running_) {
        return false;
    }
    running_ = true;
    worker_ = std::thread([this] {
        this->Worker();
    });
    return true;
}

bool chatroom::backend::MsgHandler::Stop() {
    std::unique_lock lock(lck_);
    if (!running_) {
        return false;
    }
    running_ = false;
    lock.unlock();  // *********** EXIT CRITICAL **********
    cv_.notify_all();

    worker_.join();
    // worker_ = std::thread(); // MsgHandler没有重复使用的需求
    lock.lock();
    return true;
}

void chatroom::backend::MsgHandler::Worker() {
    std::unique_lock lock(lck_);
    while (running_) {
        while (running_ && q_.empty()) {
            cv_.wait(lock);
        }
        if (!running_) {
            break;  // 这里会放弃掉剩余的未处理消息
        }
        auto item = std::move(q_.front());
        q_.pop();
        // 具体的处理过程
        Processor(std::move(item.first), std::move(item.second));
    }
}

// TODO(user): unfinished
// 消息处理逻辑
void chatroom::backend::MsgHandler::Processor(CbSessType&& sess, RcvdMsgType&& msg) {
    uint32_t msg_type = msg->GetTagField();
    switch (msg_type) {
        case DEBUG:
        {
            spdlog::debug(std::string(msg->GetContent(), msg->GetContentLen()));
        }
        case VERIFY:
        {
            spdlog::debug("Verify message received");
        }
        break;
        case CHAT_MSG:
        {
            spdlog::debug("Chat message received");
            uint64_t target_uid = ReadNetField64(msg->GetContent());
            auto target_sess = sess_mgr_->GetSession(target_uid);
            if (!target_sess) {
                // 用户未在本服务器上登陆，要么其在其他服务器上、要么其不在线、甚至可能是不存在的用户ID，最复杂的场景
                spdlog::debug("MsgHandler received a non-local user chat");
                spdlog::debug("TODO: finish the logic!");
                
            } else {
                spdlog::debug("MsgHandler received a local user chat");
                // 将消息传给对应用户即可，不需要拷贝
                msg->ResetCurPos();
                msg->SetTagField(CHAT_MSG_TOCLI);
                // set user = current uid
                WriteNetField64(msg->GetContent(), sess->GetUserId());
                // 剩余内容不需要修改
                target_sess->Send(std::move(msg));
            }
        }
        break;
        case GROUP_CHAT_MSG:
        {
            // TODO(user): 完成群聊功能
            spdlog::debug("GroupChat message received");
            uint64_t target_group;
            return;
        }
        break;
        default: 
        {
            spdlog::warn("Unknown message type: {}", msg_type);
            return; // 不知道如何处理
        }
    }
}