#ifndef SERVER_STATUS_REPORTER_HEADER
#define SERVER_STATUS_REPORTER_HEADER

// status_reporter.cpp: 后台服务器向状态服务器报告自身负载状态的类

#include "server/status_reporter.hpp"

namespace chatroom::backend {
    bool StatusReportRPCImpl::ReportLoad(uint32_t id, uint32_t load) {
        auto stub = rpc_client_->GetThreadStatusStub();
        grpc::ClientContext ctx;
        chatroom::status::StatusReportReq req;
        chatroom::status::GeneralResp resp;
        req.set_server_id(id);
        req.set_load(load);
        auto rpc_status = stub->ReportServerLoad(&ctx, req, &resp);
        return rpc_status.ok();
    }

    bool StatusReportRPCImpl::ReportServerRegister(uint32_t id, const std::string& serv_addr, uint32_t load) {
        auto stub = rpc_client_->GetThreadStatusStub();
        grpc::ClientContext ctx;
        chatroom::status::ServerRegisterReq req;
        chatroom::status::GeneralResp resp;
        req.set_server_id(id);
        req.set_server_addr(serv_addr);
        req.set_load(load);
        auto rpc_status = stub->RegisterServer(&ctx, req, &resp);
        return rpc_status.ok();
    }
    bool StatusReportRPCImpl::ReportServerLeave() {
        auto stub = rpc_client_->GetThreadStatusStub();
        // TODO(user): Implement ServerLeave interface in status service
        return false;
    }


    // ***** StatusReporter *****
    void StatusReporter::UpdateNow() {
        boost::asio::post(ctx, [this] {
            timer_.cancel();
            ReportImpl();
            ResetTimer();
        });
    }
    void StatusReporter::Register(const std::string& addr) {
        rpc_impl_.ReportServerRegister(server_id_, addr, sess_mgr_->GetSessionCount());
    }
    void StatusReporter::Stop() {
        // stopped_ = true;
        if (stopped_.exchange(true)) return;
        boost::asio::post(ctx, [this] {
            timer_.cancel();
            work_guard_.reset();
        });
        ctx.stop();
        worker_.join();
    }

    
    void StatusReporter::ReportImpl() {
        uint32_t sess_count = sess_mgr_->GetSessionCount();
        uint32_t tmps_count = sess_mgr_->GetTempSessionCount();
        spdlog::info("Reporting load: {} sessions, {} temporary sessions", sess_count, tmps_count);
        rpc_impl_.ReportLoad(server_id_, sess_count + tmps_count);
    }
    void StatusReporter::StartTimer() {
        timer_.async_wait([this](const boost::system::error_code &ec) {
            if (ec) return;
            ReportImpl();
            ResetTimer();
        });
    }
    void StatusReporter::ResetTimer() {
        timer_.expires_after(std::chrono::seconds(interval_sec_));
        StartTimer();
    }
}


#endif