#ifndef BACKEND_STATUS_REPORTER_HEADER
#define BACKEND_STATUS_REPORTER_HEADER

// status_reporter.hpp: 负责定时/手动向状态服务器上报负载的类

#include <chrono>
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>

#include <chrono>
#include <thread>
#include <utility>

#include "log/log_manager.hpp"
#include "protocpp/status.pb.h"
#include "protocpp/status.grpc.pb.h"
#include "server/rpc/status_rpc_client.hpp"
#include "server/session_manager.hpp"

namespace chatroom::backend {
    class StatusReportRPCImpl {
        private:
        std::shared_ptr<StatusRPCClient> rpc_client_;
        public:
        StatusReportRPCImpl(std::shared_ptr<StatusRPCClient> rpc_cli) : rpc_client_(std::move(rpc_cli)) {}
        grpc::Status ReportLoad(uint32_t id, uint32_t load);
        grpc::Status ReportServerRegister(uint32_t id, const std::string& serv_addr, uint32_t load);
        grpc::Status ReportServerLeave();
        StatusRPCClient& GetClient() {
            return *rpc_client_;
        }
    };


    class StatusReporter {
        public:
        StatusReporter(uint32_t server_id, std::shared_ptr<StatusRPCClient> rpc_cli, std::shared_ptr<SessionManager> sess_mgr, uint32_t interval_sec = 15) : 
        interval_sec_(interval_sec)
        , server_id_(server_id)
        , work_guard_(boost::asio::make_work_guard(ctx))
        , timer_(ctx, std::chrono::seconds(interval_sec))
        , rpc_impl_(std::move(rpc_cli))
        , sess_mgr_(std::move(sess_mgr))
        {
            worker_ = std::thread([this] {
                ctx.run();
                spdlog::info("StatusReporter worker thread exited");
            });
            StartTimer();
        }
        ~StatusReporter() {
            if (!stopped_) {
                Stop();
            }
        }

        // @brief 立即进行一次上传
        void UpdateNow();

        // @brief 进行服务器在状态服务处的登记
        bool Register(const std::string& addr);

        // @brief 停止运行
        void Stop();
        private:
        uint32_t interval_sec_;
        uint32_t server_id_;
        std::thread worker_;
        boost::asio::io_context ctx;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
        boost::asio::steady_timer timer_;
        StatusReportRPCImpl rpc_impl_;
        std::shared_ptr<SessionManager> sess_mgr_;
        std::atomic_bool stopped_{false};
        void ReportImpl();
        void StartTimer();
        void ResetTimer();
    };
}


#endif