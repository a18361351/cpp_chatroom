#ifndef BACKEND_USER_STATUS_UPLOAD_HEADER
#define BACKEND_USER_STATUS_UPLOAD_HEADER

// online_status_upload.hpp: 保存每个活跃的用户uid，并定时更新它们的在线状态
#include <cstdint>
#include <unordered_set>

#include "common/timer.hpp"
#include "log/log_manager.hpp"
#include "server/redis/server_redis.hpp"

namespace chatroom::backend {
class OnlineStatusUploader {
   public:
    OnlineStatusUploader(std::shared_ptr<RedisMgr> redis, TimerTaskManager *timer_mgr, uint32_t interval_sec = 10)
        : redis_(std::move(redis)), timer_mgr_(timer_mgr) {
        task_iter_ = timer_mgr_->CreateTimer(
            std::chrono::milliseconds(interval_sec * 1000),
            [this] {
                spdlog::debug("OnlineStatusUploader: Uploading online status (timed)");
                UploadImpl();
            },
            true);
        (*task_iter_)->Activate();
    }

    ~OnlineStatusUploader() { timer_mgr_->RemoveTimer(task_iter_); }

    bool AddSession(uint64_t uid) {
        std::unique_lock lock(lck_);
        return sess_.insert(uid).second;
    }

    bool RemoveSession(uint64_t uid) {
        std::unique_lock lock(lck_);
        sess_.erase(uid);
        removal_sess_.insert(uid);
        return true;
    }

    void UpdateNow() {
        UploadImpl();
        // 重新进行一次计时
        (*task_iter_)->Cancel();
        (*task_iter_)->Activate();
    }

   private:
    void UploadImpl() {
        bool expect = false;
        if (!in_progress_.compare_exchange_strong(expect, true)) {
            return;
        }
        std::unique_lock lock(lck_);
        std::unordered_set<uint64_t> sending_list = std::move(sess_);
        sess_.clear();
        std::unordered_set<uint64_t> erasing_list = std::move(removal_sess_);
        removal_sess_.clear();
        lock.unlock();  // ===== EXIT CRITICAL =====

        auto pl = redis_->GetPipeline();

        // updating uid
        // std::unordered_map<std::string, std::string> hm = {{"server_id", "unset"}, {"status", "online"}};
        for (auto uid : sending_list) {
            std::string key("status:");
            key += std::to_string(uid);
            // HEXPIRE key seconds [NX | XX | GT | LT] FIELDS numfields field [field ...]
            pl.command("HEXPIRE", key, 30, "FIELDS", 2, "server_id", "status");
        }

        // remove uid
        for (auto uid : erasing_list) {
            std::string key("status:");
            key += std::to_string(uid);
            pl.del(key);
        }

        spdlog::debug("OnlineStatusUploader: Updated {} users, removed {} users", sending_list.size(),
                      erasing_list.size());
        pl.exec();

        in_progress_.store(false);
    }
    std::shared_ptr<RedisMgr> redis_;
    std::unordered_set<uint64_t> sess_;          // 待更新的会话UID
    std::unordered_set<uint64_t> removal_sess_;  // 待删除的会话UID
    TimerTaskManager *timer_mgr_;
    TimerTaskManager::TaskIter task_iter_;
    std::mutex lck_;
    std::atomic_bool in_progress_;
};
}  // namespace chatroom::backend

#endif