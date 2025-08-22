#ifndef HTTP_GATEWAY_REDIS_HEADER
#define HTTP_GATEWAY_REDIS_HEADER

// gateway_redis: 将redis对象和业务对象封装在一起，提供简化的接口

#include <sw/redis++/redis.h>

#include "common/redis/base_redis_mgr.hpp"

namespace chatroom::gateway {
    // Redis类管理器：负责独占Redis对象，并管理其生命周期
    class RedisMgr : public chatroom::BaseRedisMgr {
        private:
        std::string sha_server_by_user;     // get server by userid
        public:
        
        // ctors
        RedisMgr() = default;
        
        // dtors
        ~RedisMgr() override = default;

        void RegisterScript() override;
    
    
        // @brief 将用户的Token存储到Redis中，以便用户进行登录
        void RegisterUserToken(std::string_view token, std::string_view user_id, long long ttl = 300);

        // @return {bool, string}: 操作是否成功，以及如果用户已经登录的话，其所在的服务器编号（正在登陆的情况，会返回"unset"）
        std::pair<bool, std::optional<std::string>> UserLoginAttempt(std::string_view user_id);

        // @brief 在验证成功后，更新缓存中的用户信息
        bool UpdateUserInfo(std::string_view user_id, std::string_view user_name);
    
        // @brief 发送控制消息到对应的后台服务器
        std::string SendServerKickCmd(std::string_view server_id, uint64_t uid, int max_count = 1000);
    };
}

#endif