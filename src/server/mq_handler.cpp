#include "log/log_manager.hpp"
#include "server/mq_handler.hpp"

namespace chatroom::backend {
    void MQHandler::WorkerFn() {
        unordered_map<std::string, RedisMgr::ItemStream> stms;
        std::string mq_key = "stream:server:"; mq_key += server_id_;
        while (running_) {
            // stms[mq_key].clear();
            stms.clear();
            try {
                redis_->RecvFromMsgQueueNoACK(server_id_, "0", stms, 2000);
            } catch (const sw::redis::TimeoutError& te) {
                continue;   // timeout
            }
            if (stms.empty()) continue;
            auto& stm = stms.at(mq_key);
            spdlog::debug("Handling {} messages from MQ", stm.size());
            for (const auto& item : stm) {
                // item: msg_id, optional<{{k, v}, ...}
                auto& msg = item.second;
                uint64_t from = std::stoull(msg->at("from"));
                uint64_t to = std::stoull(msg->at("to"));
                std::string_view content = msg->at("content");
                Handler(from, to, content);
            }
        }
    }

    void MQHandler::Handler(uint64_t from, uint64_t to, std::string_view content) {
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

}
