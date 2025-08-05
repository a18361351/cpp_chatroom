#ifndef BACKEND_STATUS_REPORTER_HEADER
#define BACKEND_STATUS_REPORTER_HEADER

// status_reporter.hpp: 负责定时/手动向状态服务器上报负载的类

#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>


#include "protocpp/status.pb.h"
#include "protocpp/status.grpc.pb.h"
#include "server/rpc/status_rpc_client.hpp"

namespace chatroom::backend {
    class StatusReportImpl {
        private:
        std::shared_ptr<StatusRPCClient> rpc_client_;
        public:
        StatusReportImpl(std::shared_ptr<StatusRPCClient> rpc_cli) : rpc_client_(std::move(rpc_cli)) {}
        bool ReportLoad(uint32_t id, uint32_t load) {
            auto stub = rpc_client_->GetStatusStub();
            grpc::ClientContext ctx;
            chatroom::status::StatusReportReq req;
            chatroom::status::GeneralResp resp;
            req.set_server_id(id);
            req.set_load(load);
            auto rpc_status = stub->ReportServerLoad(&ctx, req, &resp);
            return rpc_status.ok();
        }
        bool ReportServerRegister(uint32_t id, const std::string& serv_addr, uint32_t load) {
            auto stub = rpc_client_->GetStatusStub();
            grpc::ClientContext ctx;
            chatroom::status::ServerRegisterReq req;
            chatroom::status::GeneralResp resp;
            req.set_server_id(id);
            req.set_server_addr(serv_addr);
            req.set_load(load);
            auto rpc_status = stub->RegisterServer(&ctx, req, &resp);
            return rpc_status.ok();
        }
        bool ReportServerLeave() {
            auto stub = rpc_client_->GetStatusStub();
            // TODO(user): Implement ServerLeave interface in status service
            return false;
        }
        StatusRPCClient& GetClient() {
            return *rpc_client_;
        }
    };


    class StatusReporter {
        std::shared_ptr<StatusRPCClient> rpc_;
        StatusReporter() {
            rpc_ = std::make_shared<StatusRPCClient>();
        }
    };
}


#endif