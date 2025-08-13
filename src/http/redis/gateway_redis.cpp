#include <chrono>

#include "http/redis/gateway_redis.hpp"
#include "log/log_manager.hpp"
#include "utils/util_func.hpp"

constexpr const char* SERVER_LOAD = "server_load";
constexpr const char* SERVER_TABLE = "server_list";
constexpr const char* USER_TABLE = "user_list";


namespace chatroom::gateway {
    void RedisMgr::RegisterScript() {
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
    
    void RedisMgr::RegisterUserToken(std::string_view token, std::string_view user_id, long long ttl) {   // 默认5分钟的token存活时间
        std::string key("token:"); key += token;
        GetRedis().setex(key, ttl, user_id);
    }
    
    bool RedisMgr::UpdateUserInfo(std::string_view user_id, std::string_view user_name) {
        std::string key("userinfo:"); key += user_id;
        thread_local unordered_map<string, string> user_info = {{"user_name", "unset"}, {"last_login", "unset"}};
        user_info["user_name"] = user_name;
        user_info["last_login"] = std::to_string(get_timestamp_ms());
        auto ans = GetRedis().hsetex(key, user_info.begin(), user_info.end(), std::chrono::milliseconds(3600000));  // 1h
        return ans == 1;
    }
    
    std::pair<bool, std::optional<std::string>> RedisMgr::UserLoginAttempt(std::string_view user_id) {
        // TODO(user): 写成脚本最好
        std::string key("status:"); key += user_id;
        // 检查用户是否以及登录？
        auto ret = GetRedis().hget(key, "server_id");
        if (ret.has_value()) {
            // 以及登录，则返回其所在的服务器编号，实现强制下线逻辑
            return {false, ret};
        } // else
        // 否则，在Redis中更新用户状态
        thread_local unordered_map<string, string> user_data = {{"server_id", "unset"}, {"status", "unset"}};
        user_data["status"] = "verifyed";
        long long field_set = GetRedis().hsetex(key, user_data.begin(), user_data.end(), std::chrono::milliseconds(60000));
        if (field_set != 1) {
            return {false, nullopt};
        }
        return {true, nullopt};
    }

}

