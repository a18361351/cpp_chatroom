#include "common/redis/base_redis_mgr.hpp"
#include "log/log_manager.hpp"

void chatroom::BaseRedisMgr::ConnectTo(const std::string& url) {
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

void chatroom::BaseRedisMgr::ConnectTo(const sw::redis::ConnectionOptions& conn_opts, 
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
        throw;
    }
}