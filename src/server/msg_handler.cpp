#include "server/msg_handler.hpp"

#include <jsoncpp/json/json.h>

#include <string_view>

#include "log/log_manager.hpp"
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
    worker_ = std::thread([this] { this->Worker(); });
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
void chatroom::backend::MsgHandler::Processor(CbSessType &&sess, RcvdMsgType &&msg) {
    if (!sess) {
        spdlog::warn("MsgHandler: Received message with null session pointer");
        return;  // 无效的会话
    }
    if (!msg) {
        if (sess->IsVerified()) {
            spdlog::info("Session {} closed", sess->GetUserId());
            // 将用户设置为非在线状态并立即更新
            status_uploader_->RemoveSession(sess->GetUserId());
            status_uploader_->UpdateNow();
        }
        return;
    }

    uint32_t msg_type = msg->GetTagField();
    status_uploader_->AddSession(sess->GetUserId());  // 收到了用户发送的消息，我们更新其在线状态
    switch (msg_type) {
        case DEBUG: {
            // spdlog::debug(std::string(msg->GetContent(), msg->GetContentLen()));
            spdlog::info("Data received: {}", std::string(msg->GetContent(), msg->GetContentLen()));
            break;
        }
        case VERIFY: {
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
            } catch (std::exception &e) {
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
            redis_->UpdateUserStatus(server_id_, uid);
            sess->Send("Welcome to the chatroom!", VERIFY_DONE);
        } break;
        case CHAT_MSG: {
            spdlog::debug("Chat message received");
            if (!sess->IsVerified()) {
                // 未验证的用户无法发送消息
                return;
            }
            uint64_t target_uid = ReadNetField64(msg->GetContent());
            auto target_sess = sess_mgr_->GetSession(target_uid);
            if (!target_sess) {
                // 用户未在本服务器上登陆，要么其在其他服务器上、要么其不在线、甚至可能是不存在的用户ID，最复杂的场景
                spdlog::debug("MsgHandler received a non-local user chat");
                // 首先根据UID查询用户在线状态（查Redis）
                auto sid = redis_->GetUserLocation(target_uid);
                if (sid.has_value()) {
                    spdlog::debug("Sending message to server {}", sid.value());
                    // 通过消息队列（Redis Stream）发送给对方服务器
                    auto ret = redis_->SendToMsgQueue(sid.value(), sess->GetUserId(), target_uid,
                                                      std::string_view(msg->GetContent() + sizeof(uint64_t),
                                                                       msg->GetContent() + msg->GetContentLen()));
                    spdlog::debug("Message sent with return value: {}", ret);
                } else {
                    // 用户不在线，需要进一步查询（可能需要查MySQL数据库来确定用户是否存在）
                    spdlog::debug("TODO: Finish the logic for offline user!");
                }
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
        } break;
        case GROUP_CHAT_MSG: {
            // TODO(user): 完成群聊功能
            spdlog::debug("GroupChat message received");
            if (!sess->IsVerified()) {
                // 未验证的用户无法发送消息
                return;
            }
            // uint64_t target_group;
            return;
        } break;
        case PING: {
            // do nothing
        } break;
        default: {
            spdlog::warn("Unknown message type: {}", msg_type);
            return;  // 不知道如何处理
        }
    }
}