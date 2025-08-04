#ifndef SERVER_REDIS_HEADER
#define SERVER_REDIS_HEADER

#include "common/redis/base_redis_mgr.hpp"

namespace chatroom::backend {
    class RedisMgr : public chatroom::BaseRedisMgr {
        // ctors
        RedisMgr() = default;
        
        // dtors
        ~RedisMgr() override = default;

        
    };
}

#endif