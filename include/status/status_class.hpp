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

namespace chatroom {
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
        };
        StatusServiceImpl rpc_service_;

        public:
        void Run() {
            
        }
    
    };


    class StatusServer {
        StatusRPCManager rpc_;
        StatusRedisMgr redis_mgr_;
        
    };
}

#endif 