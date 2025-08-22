#ifndef COMMON_BASE_REDIS_MANAGER_HEADER
#define COMMON_BASE_REDIS_MANAGER_HEADER

// status_redis: 将redis对象和业务对象封装在一起，提供简化的接口

#include "utils/util_class.hpp"
#include <sw/redis++/command_options.h>
#include <sw/redis++/redis.h>

using namespace std;
namespace chatroom {
    // Redis类管理器：负责独占Redis对象，并管理其生命周期
    class BaseRedisMgr : public Noncopyable {
        private:
        // sw::redis::Redis是个RAII的对象且线程安全的对象，构造时自动连接，析构时自动释放
        std::unique_ptr<sw::redis::Redis> redis_;
        public:
        // @brief 获取一个Redis对象
        sw::redis::Redis& GetRedis() {
            if (!redis_) {
                throw std::runtime_error("Redis connection not initialized");
            }
            return *redis_;
        }
        public:
        // ctors
        BaseRedisMgr() = default;
        
        // dtors
        virtual ~BaseRedisMgr() = default;

        // cannot be copied
        // but movable
        BaseRedisMgr(BaseRedisMgr&& rhs) noexcept : redis_(std::move(rhs.redis_)) {
            rhs.redis_.reset();
        }
        BaseRedisMgr& operator=(BaseRedisMgr&& rhs) noexcept {
            if (this != &rhs) {
                redis_ = std::move(rhs.redis_);
                rhs.redis_.reset();
            }
            return *this;
        }

        // @brief 连接到远程服务器，已连接的情况下会重新连接，连接失败时会抛出异常
        // @param Redis服务的url
        // URI, e.g. 'tcp://127.0.0.1', 'tcp://127.0.0.1:6379', or 'unix://path/to/socket'.
        //            Full URI scheme: 'tcp://[[username:]password@]host[:port][/db]' or
        //            unix://[[username:]password@]path-to-unix-domain-socket[/db]
        void ConnectTo(const std::string& url);


        // @brief 连接到远程服务器，已连接的情况下会重新连接，连接失败时会抛出异常
        // @param conn_opts Redis连接选项
        // @param pool_opts Redis连接池的配置选项
        void ConnectTo(const sw::redis::ConnectionOptions& conn_opts, 
                       const sw::redis::ConnectionPoolOptions& pool_opts = {});

        // @brief 该Redis对象是否已连接
        bool IsConnected() const {
            return (redis_.get());
        }

        virtual void RegisterScript() {};
        // virtual void UnregisterScript() {};  // Redis不支持删除特定脚本的命令，那么这个接口实质上无法实现
    };
};

#endif