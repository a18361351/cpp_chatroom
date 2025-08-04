#include <chrono>

#include "http/redis/gateway_redis.hpp"
#include "log/log_manager.hpp"

constexpr const char* SERVER_LOAD = "server_load";
constexpr const char* SERVER_TABLE = "server_list";
constexpr const char* USER_TABLE = "user_list";

inline long long GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void chatroom::gateway::RedisMgr::RegisterScript() {
    if (!IsConnected()) {
        throw std::runtime_error("Redis connection not initialized");
    }
    const std::string get_server_by_userid = 
    "local server_id = redis.call(\"GET\", KEYS[1])\n"
    "if not server_id then\n"
    "   return nil\n"
    "end\n"
    "return redis.call(\"HGET\", " + std::string(SERVER_TABLE) + ", server_id)";
    sha_server_by_user = GetRedis().script_load(get_server_by_userid);
}

void chatroom::gateway::RedisMgr::RegisterUserToken(std::string_view token, std::string_view user_id, long long ttl) {   // 默认5分钟的token存活时间
    std::string key("token:"); key += token;
    GetRedis().setex(key, ttl, user_id);
}

std::pair<bool, std::optional<std::string>> chatroom::gateway::RedisMgr::UserLoginAttempt(std::string_view user_id) {
    // TODO(user): 写成脚本最好
    std::string key("status:"); key += user_id;
    // 检查用户是否以及登录？
    auto ret = GetRedis().hget(key, "server_id");
    if (ret.has_value()) {
        // 以及登录，则返回其所在的服务器编号，实现强制下线逻辑
        return {false, ret};
    } // else
    // 否则，在Redis中更新用户状态
    static std::unordered_map<string, string> user_data = {{"server_id", "unset"}, {"status", "unset"}, {"user_id", "unset"}, {"last_login", "unset"}};
    user_data["status"] = "verifyed";
    user_data["user_id"] = user_id;
    user_data["last_login"] = std::to_string(GetTimestamp());
    long long field_set = GetRedis().hsetex(key, user_data.begin(), user_data.end(), std::chrono::milliseconds(60000));
    if (field_set != 1) {
        return {false, nullopt};
    }
    return {true, nullopt};
}
