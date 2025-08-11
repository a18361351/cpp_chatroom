#ifndef BACKEND_USER_STATUS_UPLOAD_HEADER
#define BACKEND_USER_STATUS_UPLOAD_HEADER

// online_status_upload.hpp
#include <cstdint>
#include <unordered_set>

#include "server/redis/server_redis.hpp"

namespace chatroom::backend {
    class OnlineStatusUploader {
        public:
        bool AddSession(uint64_t uid) {
            return sess_.insert(uid).second;
        }

        private:
        void UploadImpl() {
            auto pl = redis_->GetPipeline();
            for (auto uid : sess_) {
                std::string key("status:"); key += std::to_string(uid);
                pl.expire(key, 60);
            }
            pl.exec();
            sess_.clear();
        }
        std::shared_ptr<RedisMgr> redis_;
        std::unordered_set<uint64_t> sess_; // 待更新的会话UID
    };
}


#endif 