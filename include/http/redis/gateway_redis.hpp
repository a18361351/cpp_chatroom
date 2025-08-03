#ifndef HTTP_GATEWAY_REDIS_HEADER
#define HTTP_GATEWAY_REDIS_HEADER

// gateway_redis: 将redis对象和业务对象封装在一起，提供简化的接口
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
    
            const std::string get_minimal_load_server = 
            "local server_id = redis.call(\"ZRANGE\", " + std::string(SERVER_LOAD) +", 0, 0)\n"
            "if not server_id then\n"
            "   return nil\n"
            "end\n"
            "return redis.call(\"HGET\", " + std::string(SERVER_TABLE) + ", server_id)";
            sha_min_load_server = redis_->script_load(get_minimal_load_server);
        }
    
    
        // @brief 返回最小负载的服务器id
        std::optional<std::string> QueryMinimalLoadServerId() {
            vector<string> ans;
            redis_->zrange(SERVER_LOAD, 0, 0, std::back_inserter(ans));
            if (ans.empty()) return string();
            return ans.back();
        }
    
        // @brief 返回最小负载的服务器地址
        std::optional<std::string> QueryMinimalLoadServerAddr() {
            return redis_->evalsha<std::optional<std::string>>(sha_min_load_server, {}, {});
        }
    
        // @brief 根据用户id查询其所在的服务器id
        std::optional<std::string> QueryServerIdByUser(std::string_view user_id) {
            return redis_->get(user_id);
        }
    
        // @brief 根据用户id查询其所在的服务器地址
        std::optional<std::string> QueryServerAddrByUser(std::string_view user_id) {
            return redis_->evalsha<std::optional<std::string>>(sha_server_by_user, {user_id}, {});
        }
    
        // @brief 将用户的Token存储到Redis中，以便用户进行登录
        void RegisterUserToken(std::string_view token, std::string_view user_id, long long ttl = 300) {   // 默认5分钟的token存活时间
            std::string key("token:"); key += token;
            redis_->setex(key, ttl, user_id);
        }
    };

}

#endif