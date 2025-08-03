#include <string>
#include <sys/types.h>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/support/status.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/server_builder.h>
#include <sw/redis++/connection_pool.h>

#include "log/log_manager.hpp"
#include "status/status_class.hpp"

// using namespace chatroom::status;

void chatroom::status::StatusRPCManager::Run(const std::string& rpc_address) {
    if (!server_) {
        // StatusServiceImpl service(redis_mgr_, load_balancer_);
        assert(!service_);
        service_ = std::make_unique<StatusServiceImpl>(redis_mgr_, load_balancer_);
        
        grpc::ServerBuilder builder;
        builder.AddListeningPort(rpc_address, grpc::InsecureServerCredentials());
        builder.RegisterService(service_.get());

        server_ = builder.BuildAndStart();
        // The server must be either shutting down or some other thread must call Shutdown for this function to ever return.
        server_->Wait();
    }
}

void chatroom::status::StatusRPCManager::Stop() {
    if (server_) {
        // server_需要service_，那么我们明显应该先析构server_然后再service_
        server_->Shutdown();
        server_.reset();
        service_.reset();
    }
}

bool chatroom::status::StatusServer::RunStatusServer(const string& rpc_address, const sw::redis::ConnectionOptions& conn_opt, const sw::redis::ConnectionPoolOptions& pool_opt) {
    if (!running_) {
        // Load balancer initialize
        spdlog::info("LoadBalancer init");
        load_balancer_ = std::make_unique<LoadBalancer>();

        // redis connection initialize
        redis_mgr_ = std::make_unique<RedisMgr>();
        redis_mgr_->ConnectTo(conn_opt, pool_opt);

        // RPC manager initialize
        spdlog::info("gRPC Server init");
        rpc_ = std::make_unique<StatusRPCManager>(redis_mgr_.get(), load_balancer_.get());

        spdlog::info("StatusServer starting");
        running_ = true;
        rpc_->Run(rpc_address);
        return true;
    }
    return false;
}

void chatroom::status::StatusServer::StopStatusServer() {
    if (running_) {
        rpc_->Stop();
        rpc_.reset();
        redis_mgr_.reset();
        load_balancer_.reset();
        running_ = false;
        spdlog::info("StatusServer stopped");
    }
}