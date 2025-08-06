#include "server/redis/server_redis.hpp"
#include <absl/strings/numbers.h>

namespace chatroom::backend {
    std::optional<uint64_t> RedisMgr::VerifyUser(std::string_view token) {
        std::string key("token:"); key += token;
        auto ret = GetRedis().get(key);
        if (ret == nullopt) {
            return nullopt;
        }
        uint64_t uid = std::stoull(ret.value());
        return uid;
    }
}