#include "server/session.hpp"
#include "server/session_manager.hpp"

bool chatroom::backend::SessionManager::StopSession(UID sess_id) {
    std::unique_lock lock(lck_);
    auto it = sess_.find(sess_id);
    if (it != sess_.end()) {
        it->second->down_ = true;
        it->second->sock_.close();
        sess_.erase(it);
        return true;
    }
    return false;
}
        // @brief 添加Session对象到管理器中
bool chatroom::backend::SessionManager::AddSession(UID sess_id, std::shared_ptr<Session> sess) {
    std::unique_lock lock(lck_);
    if (sess_.find(sess_id) != sess_.end()) {
        return false;
    }
    sess_.insert({sess_id, std::move(sess)});
    return true;
}

// @brief 获取当前SessionManager中所有有效的Session的个数
uint32_t chatroom::backend::SessionManager::GetSessionCount() const {
    std::unique_lock lock(lck_);
    return sess_.size();
}

std::shared_ptr<chatroom::backend::Session> chatroom::backend::SessionManager::GetSession(UID sess_id) {
    std::unique_lock lock(lck_);
    auto it = sess_.find(sess_id);
    if (it != sess_.end()) {
        return it->second;
    }
    return nullptr;
}