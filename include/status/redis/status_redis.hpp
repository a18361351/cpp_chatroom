#ifndef HTTP_STATUS_REDIS_HEADER
#define HTTP_STATUS_REDIS_HEADER

// status_redis: 将redis对象和业务对象封装在一起，提供简化的接口

#include <unordered_map>

#include "common/redis/base_redis_mgr.hpp"

using namespace std;
namespace chatroom::status {
    // Redis类管理器：负责独占Redis对象，并管理其生命周期
    class RedisMgr : public chatroom::BaseRedisMgr {
        public:
        // ctors
        RedisMgr() = default;

        // dtors
        ~RedisMgr() override = default;

        void RegisterScript() override;
    
        // @brief 状态服务器同时将自己的服务器列表上传至Redis服务器
        bool UpdateServerList(std::unordered_map<std::string, std::string>& serv_list);

    };
};

#endif