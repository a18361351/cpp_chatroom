#ifndef SERVER_STATUS_REPORTER_HEADER
#define SERVER_STATUS_REPORTER_HEADER

// status_reporter.cpp: 后台服务器向状态服务器报告自身负载状态的类

#include "server/status_reporter.hpp"

#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>

namespace chatroom::backend {
grpc::Status StatusReportRPCImpl::ReportLoad(uint32_t id, uint32_t load) {
    auto stub = rpc_client_->GetThreadStatusStub();
    grpc::ClientContext ctx;
    chatroom::status::StatusReportReq req;
    chatroom::status::GeneralResp resp;
    req.set_server_id(id);
    req.set_load(load);
    auto rpc_status = stub->ReportServerLoad(&ctx, req, &resp);
    return rpc_status;
}

grpc::Status StatusReportRPCImpl::ReportServerRegister(uint32_t id, const std::string &serv_addr, uint32_t load) {
    auto stub = rpc_client_->GetThreadStatusStub();
    grpc::ClientContext ctx;
    chatroom::status::ServerRegisterReq req;
    chatroom::status::GeneralResp resp;
    req.set_server_id(id);
    req.set_server_addr(serv_addr);
    req.set_load(load);
    auto rpc_status = stub->RegisterServer(&ctx, req, &resp);
    return rpc_status;
}
grpc::Status StatusReportRPCImpl::ReportServerLeave() {
    // auto stub = rpc_client_->GetThreadStatusStub();
    // TODO(user): Implement ServerLeave interface in status service
    return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

// ***** StatusReporter *****
void StatusReporter::UpdateNow() {
    (*task_iter_)->Cancel();
    ReportImpl();
    (*task_iter_)->Activate();
}
bool StatusReporter::Register() {
    auto ret = rpc_impl_.ReportServerRegister(server_id_, server_addr_, sess_mgr_->GetSessionCount());
    if (!ret.ok()) {
        spdlog::error("StatusReporter register rpc call failed: {}", ret.error_message());
    }
    return ret.ok();
}
void StatusReporter::Stop() {
    // stopped_ = true;
    if (stopped_.exchange(true)) return;
    (*task_iter_)->Cancel();
    timer_mgr_->RemoveTimer(task_iter_);
}

void StatusReporter::ReportImpl() {
    uint32_t sess_count = sess_mgr_->GetSessionCount();
    uint32_t tmps_count = sess_mgr_->GetTempSessionCount();
    spdlog::info("Reporting load: {} sessions, {} temporary sessions", sess_count, tmps_count);
    auto ret = rpc_impl_.ReportLoad(server_id_, sess_count + tmps_count);
    if (!ret.ok()) {
        if (ret.error_code() == grpc::StatusCode::NOT_FOUND) {
            // server with that id not found
            auto ret2 = rpc_impl_.ReportServerRegister(server_id_, server_addr_, sess_count + tmps_count);
            if (!ret2.ok()) {
                spdlog::error("StatusReporter re-register rpc call failed: {}", ret2.error_message());
            }
        } else {
            spdlog::error("StatusReporter report rpc call failed: {}", ret.error_message());
        }
    }
}
void StatusReporter::StartTimer() { (*task_iter_)->Activate(); }
void StatusReporter::ResetTimer() {
    (*task_iter_)->Cancel();
    (*task_iter_)->Activate();
}
}  // namespace chatroom::backend

#endif