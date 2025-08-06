#ifndef SERVER_REDIS_HEADER
#define SERVER_REDIS_HEADER

#include "common/redis/base_redis_mgr.hpp"

namespace chatroom::backend {
    class RedisMgr : public chatroom::BaseRedisMgr {
        public:
        // ctors
        RedisMgr() = default;
        
        // dtors
        ~RedisMgr() override = default;

        std::optional<uint64_t> VerifyUser(std::string_view token);
    };
}

#endif