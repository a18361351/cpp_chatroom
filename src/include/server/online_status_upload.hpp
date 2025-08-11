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
        OnlineStatusUploader(std::shared_ptr<RedisMgr> redis, TimerTaskManager* timer_mgr, uint32_t interval_sec = 10) : redis_(std::move(redis)), timer_mgr_(timer_mgr) {
            task_iter_ = timer_mgr_->CreateTimer(std::chrono::milliseconds(interval_sec * 1000), [this] {
                spdlog::info("OnlineStatusUploader: Uploading online status (timed)");
                UploadImpl();
            }, true);
            (*task_iter_)->Activate();
        }

        ~OnlineStatusUploader() {
            timer_mgr_->RemoveTimer(task_iter_);
        }

        bool AddSession(uint64_t uid) {
            std::unique_lock lock(lck_);
            return sess_.insert(uid).second;
        }

        private:
        void UploadImpl() {
            std::unique_lock lock(lck_);
            std::unordered_set<uint64_t> sending_list_ = std::move(sess_);
            sess_.clear();
            lock.unlock();  // ===== EXIT CRITICAL =====

            auto pl = redis_->GetPipeline();
            std::unordered_map<std::string, std::string> hm = {{"server_id", "unset"}, {"status", "online"}};
            for (auto uid : sending_list_) {
                std::string key("status:"); key += std::to_string(uid);
                // HEXPIRE key seconds [NX | XX | GT | LT] FIELDS numfields field [field ...]
                pl.command("HEXPIRE", key, 30, "FIELDS", 2, "server_id", "status");
            }
            spdlog::info("OnlineStatusUploader: Updated {} users", sending_list_.size());
            pl.exec();
        }
        std::shared_ptr<RedisMgr> redis_;
        std::unordered_set<uint64_t> sess_; // 待更新的会话UID
        TimerTaskManager* timer_mgr_;
        TimerTaskManager::TaskIter task_iter_;
        std::mutex lck_;
    };
}


#endif 