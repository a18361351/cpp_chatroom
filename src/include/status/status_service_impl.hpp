#ifndef STATUS_SERVICE_IMPL_HEADER
#define STATUS_SERVICE_IMPL_HEADER

#include "protocpp/status.grpc.pb.h"
#include "protocpp/status.pb.h"
#include "status/load_balancer.hpp"
#include "status/redis/status_redis.hpp"
#include "status/status_uploader.hpp"

namespace chatroom::status {

class StatusServiceImpl final : public StatusService::Service {
    grpc::Status ReportServerLoad(grpc::ServerContext *ctx, const StatusReportReq *request,
                                  GeneralResp *response) override {
        auto srv = request->server_id();
        auto load = request->load();
        bool ret = balancer_->UpdateServerLoad(srv, load);
        if (ret) {
            response->set_ret(0);
            return grpc::Status::OK;
        } else {
            response->set_ret(1);  // 更新失败
            return {grpc::StatusCode::NOT_FOUND, "Couldn't found server with that ID."};
        }
    }
    grpc::Status CheckUserOnline(grpc::ServerContext *context, const UserCheckReq *request,
                                 GeneralResp *response) override {
        return {grpc::StatusCode::UNIMPLEMENTED, "CheckUserOnline not implemented yet."};
    }
    grpc::Status CheckMinimalLoadServer(grpc::ServerContext *context, const MinimalLoadServerReq *request,
                                        ServerAddrResp *response) override {
        // auto srv_addr = redis_mgr_->QueryMinimalLoadServerAddr();
        auto [si, updated] = balancer_->GetMinimalLoadServerInfo();
        if (!si.has_value()) {
            response->set_ret(1);
            return {grpc::StatusCode::NOT_FOUND, "No server currently available."};
        }
        if (updated) {
            uploader_->UpdateNow();  // 显式更新一次服务器列表（异步）
        }
        response->set_server_id(si->id);
        response->set_server_addr(si->addr);
        response->set_ret(0);
        return grpc::Status::OK;
    }
    grpc::Status RegisterServer(grpc::ServerContext *context, const ServerRegisterReq *request,
                                GeneralResp *response) override {
        bool ret = balancer_->RegisterServerInfo(request->server_id(), request->server_addr(), request->load());
        if (ret) {
            response->set_ret(0);
            uploader_->UpdateNow();  // 显式更新一次服务器列表（异步）
            return grpc::Status::OK;
        } else {
            response->set_ret(1);  // 注册失败
            return {grpc::StatusCode::UNKNOWN, "Unknown error in register."};
        }
    }
    grpc::Status KickOnlineUser(grpc::ServerContext *context, const KickRequest *request,
                                GeneralResp *response) override {
        return {grpc::StatusCode::UNIMPLEMENTED, "KickOnlineUser not implemented yet."};
    }

    grpc::Status DumpServerList(grpc::ServerContext *context, const DumpServerListReq *request,
                                ServerItemListResp *response) override {
        std::vector<ServerInfo> out;
        balancer_->CopyServerInfoList(out);
        for (auto &item : out) {
            auto *server_item = response->add_servers();
            server_item->set_id(item.id);
            server_item->set_addr(item.addr);
            server_item->set_load(item.load);
            server_item->set_last_ts(item.last_ts);
        }
        response->set_ret(0);
        return grpc::Status::OK;
    }

   public:
    // ctor
    StatusServiceImpl(RedisMgr *redis, LoadBalancer *load_balancer, TimedUploader *uploader)
        : redis_mgr_(redis), balancer_(load_balancer), uploader_(uploader) {}

    // dtor
    ~StatusServiceImpl() override = default;

   private:
    RedisMgr *redis_mgr_;
    LoadBalancer *balancer_;
    TimedUploader *uploader_;
};
}  // namespace chatroom::status

#endif