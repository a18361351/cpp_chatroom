#ifndef HTTP_STATUS_REDIS_HEADER
#define HTTP_STATUS_REDIS_HEADER

// status_redis: 将redis对象和业务对象封装在一起，提供简化的接口

#include <iterator>
#include <stdexcept>

#include <sw/redis++/command_options.h>
#include <sw/redis++/redis.h>
#include <boost/asio/thread_pool.hpp>

#include "log/log_manager.hpp"

constexpr const char* SERVER_LOAD = "server_load";
constexpr const char* SERVER_TABLE = "server_list";
constexpr const char* USER_TABLE = "user_list";

using namespace std;
namespace chatroom::status {
    // Redis类管理器：负责独占Redis对象，并管理其生命周期
    class RedisMgr {
        private:
        // sw::redis::Redis是个RAII的对象，构造时自动连接，析构时自动释放
        std::unique_ptr<sw::redis::Redis> redis_;
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
            // 加载脚本
            // script_xxx = redis_->script_load("...");
        }
    
        // @brief 状态服务器同时将自己的服务器列表上传至Redis服务器
        bool UpdateServerList(std::unordered_map<std::string, std::string>& serv_list) {
            long long ans = redis_->hsetex("server_list", serv_list.begin(), serv_list.end(), std::chrono::milliseconds(40000));
            return (ans == serv_list.size());
        }

    };
};

#endif