#include <jsoncpp/json/json.h>

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
    spdlog::info("MsgHandler worker thread exited");
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
            // spdlog::debug(std::string(msg->GetContent(), msg->GetContentLen()));
            spdlog::info("Data received: {}", std::string(msg->GetContent(), msg->GetContentLen()));
            break;
        }
        case VERIFY:
        {
            spdlog::debug("Verify message received");
            if (sess->IsVerified()) {
                // 无需重复验证
                return;
            }
            Json::Value root;
            Json::Reader reader;
            std::string token;
            uint64_t uid;
            if (!reader.parse(msg->GetContent(), root)) {
                // 无效的验证请求，其无法被解析为JSON
                spdlog::error("Invalid verify message");
                sess->Close();  // 关闭会话
                sess_mgr_->RemoveTempSession(sess.get());
                return;
            }
            try {
                token = root["token"].asString();
                uid = root["uid"].asUInt64();
            } catch (std::exception& e) {
                spdlog::error("Invalid verify message");
                sess->Close();  // 关闭会话
                sess_mgr_->RemoveTempSession(sess.get());
                return;
            }
            // 验证过程
            std::optional<uint64_t> ans_uid = redis_->VerifyUser(token);
            if (!ans_uid.has_value() || ans_uid != uid) {
                // 错误或过期的token
                spdlog::error("User attempt to verify with wrong/expired token");
                sess->Close();  // 关闭会话
                sess_mgr_->RemoveTempSession(sess.get());
                return;
            }
            // 否则验证成功
            sess->SetVerified(uid);
            sess_mgr_->AddSession(uid, sess);
            sess->Send("Welcome to the chatroom!", VERIFY_DONE);
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