#include "server/logic.hpp"

void LogicSys::DealMsg() {
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

// Callbacks
void LogicSys::WelcomeMsgCallback(CbSessType sess, RcvdMsgType msg) {
    sess->Send("Welcome to the server!", 0);
}
void LogicSys::TextMsgCallback(CbSessType sess, RcvdMsgType msg) {
    fwrite(msg->GetContent(), 1, msg->GetContentLen(), stdout);
    fflush(stdout);
}
void LogicSys::EchoMsgCallback(CbSessType sess, RcvdMsgType msg) {
    // 回显消息
    sess->Send(msg->GetContent(), msg->GetContentLen(), ECHO_MSG);
}