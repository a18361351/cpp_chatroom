#ifndef STATUS_CLASS_HEADER
#define STATUS_CLASS_HEADER

#include <grpcpp/server.h>

#include "status/load_balancer.hpp"
#include "status/redis/status_redis.hpp"
#include "status/status_service_impl.hpp"

namespace chatroom::status {
    // 同步RPC的写法
    class StatusRPCManager {
        private:
        RedisMgr* redis_mgr_;     // 不负责其生命周期管理
        LoadBalancer* load_balancer_;   

        std::unique_ptr<grpc::Server> server_;
        std::unique_ptr<StatusServiceImpl> service_;      // FIXME
        public:
        // FIXME: 线程安全
        // @brief 启动RPC服务，调用后线程会阻塞于此函数，直到其他线程调用Stop函数
        void Run(const std::string& rpc_address);
    
        // FIXME: 线程安全
        // @brief 停止RPC服务的运行，该函数应该由其他线程调用
        void Stop();

        // ctor
        StatusRPCManager(RedisMgr* redis, LoadBalancer* load_balancer) :
                redis_mgr_(redis), load_balancer_(load_balancer) {}
        
        // dtor
        ~StatusRPCManager() = default;
    
    };


    class StatusServer {
        // 注意：StatusServer负责管理redis对象、load_balancer以及rpc对象的生命周期，同时内部使用了大量的裸指针。
        //      在同步的情况下没有问题，但是在异步的情况下，会出现对象生命周期无法管理的问题（对象销毁时，还有正在
        //      进行的回调可能访问对象），此时需要修改其资源管理的方式。
        private:
        std::unique_ptr<StatusRPCManager> rpc_;
        std::unique_ptr<RedisMgr> redis_mgr_;
        std::unique_ptr<LoadBalancer> load_balancer_;
        bool running_{false};
        public:

        // ctor
        StatusServer() = default;

        // @brief 启动状态服务
        // @warning 这个函数是阻塞的……要停止该服务，请在其他线程处调用StopStatusServer()
        bool RunStatusServer(const string& rpc_address, const sw::redis::ConnectionOptions& conn_opt, const sw::redis::ConnectionPoolOptions& pool_opt);

        // @brief 停止状态服务
        void StopStatusServer();
    };
}

#endif 