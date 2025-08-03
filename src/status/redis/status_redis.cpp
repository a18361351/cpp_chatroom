#include "status/redis/status_redis.hpp"


void chatroom::status::RedisMgr::ConnectTo(const std::string& url) {
    spdlog::info("RedisMgr connecting to {}", url);
    if (redis_) {
        redis_.reset();
    }
    redis_ = std::make_unique<sw::redis::Redis>(url);   // may throw
    try {
        spdlog::info("RedisMgr connection established");
        RegisterScript();
        spdlog::info("RedisMgr script registering");
    } catch (const std::exception& e) {
        spdlog::error("RedisMgr::ConnectTo() throws an exception: {}", e.what());
        redis_.reset();
        throw;
    }
}

void chatroom::status::RedisMgr::ConnectTo(const sw::redis::ConnectionOptions& conn_opts, 
               const sw::redis::ConnectionPoolOptions& pool_opts) 
{                
    spdlog::info("RedisMgr connecting to {}", conn_opts.host);
    if (redis_) {
        redis_.reset();
    }
    redis_ = std::make_unique<sw::redis::Redis>(conn_opts, pool_opts);   // may throw
    try {
        spdlog::info("RedisMgr connection established");
        RegisterScript();
        spdlog::info("RedisMgr script registering");
    } catch (const std::exception& e) {
        spdlog::error("RedisMgr::ConnectTo() throws an exception: {}", e.what());
        redis_.reset();
        throw e;
    }
}

void chatroom::status::RedisMgr::UnregisterScript() {
    if (!redis_) {
        return;
    }
    redis_->script_flush();
}

void chatroom::status::RedisMgr::RegisterScript() {
    if (!redis_) {
        throw std::runtime_error("Redis connection not initialized");
    }
    // 加载脚本
    // script_xxx = redis_->script_load("...");
}

bool chatroom::status::RedisMgr::UpdateServerList(std::unordered_map<std::string, std::string>& serv_list) {
    if (serv_list.empty()) {
        // 空服务器列表不需要上传
        auto pong = redis_->ping();
        return !pong.empty();
    }
    long long ans = redis_->hsetex("server_list", serv_list.begin(), serv_list.end(), std::chrono::milliseconds(40000));
    return (ans == serv_list.size());
}
