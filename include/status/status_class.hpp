#ifndef STATUS_CLASS_HEADER
#define STATUS_CLASS_HEADER

#include <sys/types.h>

#include <grpcpp/support/status.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/async_unary_call.h>
#include "protocpp/status.grpc.pb.h"
#include "protocpp/status.pb.h"

#include "status/redis/status_redis.hpp"
#include "status/load_balancer.hpp"

namespace chatroom {
    // FIXME(user): complete everything
    class StatusRPCManager {
        private:
        class CallData {
            public:
            CallData(StatusService::AsyncService* service, grpc::ServerCompletionQueue* cq) 
                {}

            void Request() {
                
            }
            private:
            StatusService::AsyncService* service_;
            grpc::ServerCompletionQueue* cq_;
            grpc::ServerContext ctx_;
            enum State {
                CREATE,
                PROCESS,
                FINISH
            };
        };


        private:
        // 同步RPC的写法
        class StatusServiceImpl final : public StatusService::Service {
            grpc::Status ReportServerLoad(grpc::ServerContext* ctx, const StatusReportReq* request, GeneralResp* response) override {
                auto srv = request->server_id();
                auto load = request->load();
                bool ret = load_balancer_->UpdateServerLoad(srv, load);
                if (ret) {
                    response->set_ret(0);
                } else {
                    response->set_ret(1); // 更新失败
                }
                return grpc::Status::OK;
            }
            grpc::Status CheckUserOnline(grpc::ServerContext* context, const UserCheckReq* request, GeneralResp* response) override {
                
            }
            grpc::Status CheckMinimalLoadServer(grpc::ServerContext* context, const MinimalLoadServerReq* request, ServerAddrResp* response) override {
                auto srv_addr = redis_mgr_->QueryMinimalLoadServerAddr();
                if (!srv_addr.has_value()) {
                    response->set_ret(1);
                    return grpc::Status::OK;
                }
                response->set_server_addr(srv_addr.value());
                response->set_ret(0);
                return grpc::Status::OK;
            }
            private:
            std::shared_ptr<StatusRedisMgr> redis_mgr_;
            LoadBalancer* load_balancer_;
        };
        StatusServiceImpl rpc_service_;
        LoadBalancer* load_balancer_;   // 不负责其生命周期管理
        public:
        void Run() {
            
        }
    
    };


    class StatusServer {
        private:
        StatusRPCManager rpc_;
        std::shared_ptr<sw::redis::Redis> redis_obj_;
        std::unique_ptr<StatusRedisMgr> redis_mgr_;
        std::unique_ptr<LoadBalancer> load_balancer_;
        bool running_;
        public:
        bool RunStatusServer() {
            if (!running_) {
                // Load balancer initialize
                load_balancer_ = std::make_unique<LoadBalancer>();

                // redis connection initialize
                redis_obj_ = std::make_shared<sw::redis::Redis>();
                redis_mgr_ = std::make_unique<StatusRedisMgr>(redis_obj_);

                rpc_.Run();
                return true;
            }
            return false;
        }
        void StopStatusServer() {
            if (running_) {
                rpc_.Stop();
                load_balancer_.reset();
            }
        }
    };
}

#endif 