#include "status/redis/status_redis.hpp"


void chatroom::status::RedisMgr::RegisterScript() {
    if (!IsConnected()) {
        throw std::runtime_error("Redis connection not initialized");
    }
    // 加载脚本
    // script_xxx = redis_->script_load("...");
}

bool chatroom::status::RedisMgr::UpdateServerList(std::unordered_map<std::string, std::string>& serv_list) {
    if (serv_list.empty()) {
        // 空服务器列表不需要上传
        auto pong = GetRedis().ping();
        return !pong.empty();
    }
    long long ans = GetRedis().hsetex("server_list", serv_list.begin(), serv_list.end(), std::chrono::milliseconds(40000));
    return ans;
}
