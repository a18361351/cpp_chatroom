#include "server/mq_handler.hpp"

#include "log/log_manager.hpp"

namespace chatroom::backend {
void MQHandler::WorkerFn() {
    unordered_map<std::string, RedisMgr::ItemStream> stms;
    std::string mq_key = "stream:server:";
    mq_key += server_id_;
    std::string mq2_key = "stream:serverctl:";
    mq2_key += server_id_;
    while (running_) {
        // stms[mq_key].clear();
        stms.clear();
        try {
            redis_->RecvFromMsgQueueNoACK(server_id_, "0", stms, 2000);
        } catch (const sw::redis::TimeoutError &te) {
            continue;  // timeout
        }
        if (stms.empty()) continue;
        for (auto &stm : stms) {
            if (stm.first == mq_key) {
                for (auto &item : stm.second) {
                    MessageHandler(item);
                }
            } else if (stm.first == mq2_key) {
                for (auto &item : stm.second) {
                    CtrlMsgHandler(item);
                }
            } else {
                spdlog::warn("Unknown message queue: {}", stm.first);
            }
        }
    }
}

// type = {"kick", ...}
void MQHandler::CtrlMsgHandler(RedisMgr::Item &item) {
    if (!item.second.has_value()) return;
    auto &msg = item.second.value();
    std::string_view type = msg.at("type");
    if (type == "kick") {
        uint64_t uid = std::stoull(msg.at("uid"));
        spdlog::info("Kicking user {} from server {}", uid, server_id_);
        auto sess = sess_->GetSession(uid);
        if (sess) {
            sess->Close();  // 关闭会话
        } else {
            spdlog::warn("Kick command can't find user with uid {}", uid);
        }
    } else {
        spdlog::warn("Unknown control message type: {}", type);
    }
}

void MQHandler::MessageHandler(RedisMgr::Item &item) {
    if (!item.second.has_value()) return;
    auto &msg = item.second.value();
    uint64_t from = std::stoull(msg.at("from"));
    uint64_t to = std::stoull(msg.at("to"));
    std::string_view content = msg.at("content");
    // HEAD_LEN | FROM_UID | CONTENT
    auto sending = std::make_shared<MsgNode>(HEAD_LEN + sizeof(uint64_t) + content.size());
    WriteNetField64(sending->GetContent(), from);
    std::memcpy(sending->GetContent() + sizeof(uint64_t), content.data(), content.size());
    sending->SetTagField(CHAT_MSG_TOCLI);
    sending->SetContentLenField(sizeof(uint64_t) + content.size());
    auto sess = sess_->GetSession(to);
    if (sess) {
        spdlog::debug("MQHandler: Sending msg from {} to {}", from, to);
        sess->Send(std::move(sending));
    } else {
        spdlog::error("MQHandler: User {} not on local server, message dropped", to);
    }
}

}  // namespace chatroom::backend
