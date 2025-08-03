#ifndef HTTP_GATEWAY_REDIS_HEADER
#define HTTP_GATEWAY_REDIS_HEADER

// gateway_redis: 将redis对象和业务对象封装在一起，提供简化的接口
#include <chrono>
#include <iterator>

#include <stdexcept>
#include <sw/redis++/command_options.h>
#include <sw/redis++/connection.h>
#include <sw/redis++/connection_pool.h>
#include <sw/redis++/redis.h>
#include <boost/asio/thread_pool.hpp>

#include "log/log_manager.hpp"

constexpr const char* SERVER_LOAD = "server_load";
constexpr const char* SERVER_TABLE = "server_list";
constexpr const char* USER_TABLE = "user_list";

using namespace std;

inline long long GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

namespace chatroom::gateway {
    // Redis类管理器：负责独占Redis对象，并管理其生命周期
    class RedisMgr {
        private:
        // sw::redis::Redis是个RAII的对象，构造时自动连接，析构时自动释放
        std::unique_ptr<sw::redis::Redis> redis_;
        std::string sha_server_by_user;     // get server by userid
        std::string sha_min_load_server;    // get server with minimal load
        public:
        
        // ctors
        RedisMgr() = default;
        explicit RedisMgr(const std::string& url) {
            ConnectTo(url);
        }
        
        // dtors
        ~RedisMgr() {
            if (redis_) {
                try {
                    UnregisterScript();
                } catch (const std::exception& e) {
                    spdlog::error("~RedisMgr() throws an exception: {}", e.what());
                }
                redis_.reset();
            }
        }

        // @brief 连接到远程服务器，已连接的情况下会重新连接，连接失败时会抛出异常
        // @param Redis服务的url
        // URI, e.g. 'tcp://127.0.0.1', 'tcp://127.0.0.1:6379', or 'unix://path/to/socket'.
        //            Full URI scheme: 'tcp://[[username:]password@]host[:port][/db]' or
        //            unix://[[username:]password@]path-to-unix-domain-socket[/db]
        void ConnectTo(const std::string& url) {
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

        void ConnectTo(const sw::redis::ConnectionOptions& conn_opts, 
                       const sw::redis::ConnectionPoolOptions& pool_opts = {}) 
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


        void UnregisterScript() {
            if (!redis_) {
                return;
            }
            redis_->script_flush();
        }
        
        void RegisterScript() {
            if (!redis_) {
                throw std::runtime_error("Redis connection not initialized");
            }
            const std::string get_server_by_userid = 
            "local server_id = redis.call(\"GET\", KEYS[1])\n"
            "if not server_id then\n"
            "   return nil\n"
            "end\n"
            "return redis.call(\"HGET\", " + std::string(SERVER_TABLE) + ", server_id)";
            sha_server_by_user = redis_->script_load(get_server_by_userid);
        }
    
    
        // @brief 将用户的Token存储到Redis中，以便用户进行登录
        void RegisterUserToken(std::string_view token, std::string_view user_id, long long ttl = 300) {   // 默认5分钟的token存活时间
            std::string key("token:"); key += token;
            redis_->setex(key, ttl, user_id);
        }

        // @return {bool, string}: 操作是否成功，以及如果用户已经登录的话，其所在的服务器编号
        std::pair<bool, std::optional<std::string>> UserLoginAttempt(std::string_view user_id) {
            // TODO(user): 写成脚本最好
            std::string key("status:"); key += user_id;

            // 检查用户是否以及登录？
            auto ret = redis_->hget(key, "server_id");
            if (ret.has_value()) {
                // 以及登录，则返回其所在的服务器编号，实现强制下线逻辑
                return {false, ret};
            } // else
            // 否则，在Redis中更新用户状态
            static std::unordered_map<string, string> user_data = {{"server_id", "unset"}, {"status", "unset"}, {"user_id", "unset"}, {"last_login", "unset"}};
            user_data["status"] = "verifyed";
            user_data["user_id"] = user_id;
            user_data["last_login"] = std::to_string(GetTimestamp());

            long long field_set = redis_->hsetex(key, user_data.begin(), user_data.end(), std::chrono::milliseconds(60000));
            if (field_set != user_data.size()) {
                return {false, nullopt};
            }
            return {true, nullopt};
        }
    };

}

#endif