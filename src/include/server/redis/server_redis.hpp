#ifndef SERVER_REDIS_HEADER
#define SERVER_REDIS_HEADER

#include "common/redis/base_redis_mgr.hpp"
#include <string_view>
#include <sw/redis++/pipeline.h>
#include <sw/redis++/queued_redis.h>

namespace chatroom::backend {
    class RedisMgr : public chatroom::BaseRedisMgr {
        public:
        // ctors
        RedisMgr() = default;
        
        // dtors
        ~RedisMgr() override = default;

        // Pipeline
        sw::redis::Pipeline GetPipeline() {
            return std::move(GetRedis().pipeline(false));
        }

        std::optional<uint64_t> VerifyUser(std::string_view token);
        
        bool UploadUserStatus(std::string_view user_id, std::string_view server_id) {
            std::string key("status:"); key += user_id;
            std::unordered_map<string_view, string_view> data;
            data["server_id"] = server_id;
            data["status"] = "online";
            data["user_id"];    // 刷新TTL
            data["user_name"];  // 刷新TTL
            data["last_login"]; // 刷新TTL
            long long field_set = GetRedis().hsetex(key, data.begin(), data.end(), std::chrono::milliseconds(60000));
            return field_set == 1;
        }
        private:
        std::unique_ptr<sw::redis::Pipeline> pl_;
    };
}

#endif