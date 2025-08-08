#include "log/log_manager.hpp"
#include "server/session.hpp"
#include "server/session_manager.hpp"

namespace chatroom::backend {
    bool chatroom::backend::SessionManager::StopSession(UID sess_id) {
        std::unique_lock lock(lck_);
        auto it = sess_.find(sess_id);
        if (it != sess_.end()) {
            it->second->Close();
            return true;
        }
        return false;
    }

    bool SessionManager::RemoveSession(UID sess_id) {
        std::unique_lock lock(lck_);
        return sess_.erase(sess_id) == 1;
    }
    
    bool SessionManager::AddTempSession(std::shared_ptr<Session> sess) {
        std::unique_lock lock(temp_lck_);
        auto key = sess.get();
        // 等待到验证之后，我们才能够把其放到根据UID编号的数据结构中
        return temp_sess_.try_emplace(key, std::move(sess)).second;
    }
    
    bool SessionManager::AddSession(UID sess_id, std::shared_ptr<Session> sess) {
        std::unique_lock lock(lck_);
        std::unique_lock temp_lock(temp_lck_);

        // 将会话从临时列表中移除
        if (temp_sess_.erase(sess.get()) == 0) {
            // not present in temporary list, whatever
            spdlog::warn("Verified Session not found in temporary list");
        }
        temp_lock.unlock(); // 解锁，减小锁粒度

        // 尝试插入 UID -> SessPtr
        return sess_.try_emplace(sess_id, std::move(sess)).second;
    }
    
    bool SessionManager::RemoveTempSession(Session* sess_ptr) {
        std::unique_lock temp_lock(temp_lck_);
        return temp_sess_.erase(sess_ptr) == 1;
    }

    uint32_t SessionManager::GetSessionCount() {
        std::unique_lock lock(lck_);
        return sess_.size();
    }

    uint32_t SessionManager::GetTempSessionCount() {
        std::unique_lock lock(temp_lck_);
        return temp_sess_.size();
    }

    uint32_t SessionManager::GetTotalSessionCount() {
        std::unique_lock lock(lck_);
        std::unique_lock temp_lock(temp_lck_);
        return sess_.size() + temp_sess_.size();
    }
    
    std::shared_ptr<Session> SessionManager::GetSession(UID sess_id) {
        std::unique_lock lock(lck_);
        auto it = sess_.find(sess_id);
        if (it != sess_.end()) {
            return it->second;
        }
        return nullptr;
    }
}
