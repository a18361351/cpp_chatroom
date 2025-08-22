#ifndef SERVER_STATUS_RPC_CLIENT
#define SERVER_STATUS_RPC_CLIENT

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "protocpp/status.grpc.pb.h"
#include "protocpp/status.pb.h"

namespace chatroom::backend {
    // 同步RPC的写法
    class StatusRPCClient {
        private:
        std::shared_ptr<grpc::Channel> ch_status_;
        // std::unique_ptr<status::StatusService::Stub> status_;
        public:
        // ctor
        // 建立与远端的RPC连接
        explicit StatusRPCClient(const std::string& status_addr) {
            ch_status_ = grpc::CreateChannel(status_addr, grpc::InsecureChannelCredentials());
        }

        // dtor
        ~StatusRPCClient() = default;

        // @brief 获取对应RPC Channel的线程专属存根（Stub）对象
        // @warning 请不要跨线程传递该Stub指针
        status::StatusService::Stub* GetThreadStatusStub() {
            if (ch_status_) {
                thread_local std::unique_ptr<status::StatusService::Stub> local_stub = std::make_unique<status::StatusService::Stub>(ch_status_);
                return local_stub.get();    // NOLINT
            }
            return nullptr;
        }
    };
}



#endif