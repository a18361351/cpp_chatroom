#ifndef SERVER_RPC_CLIENT_HEADER
#define SERVER_RPC_CLIENT_HEADER

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "protocpp/status.grpc.pb.h"
#include "protocpp/status.pb.h"

namespace chatroom::gateway {
    // 同步RPC的写法
    class StatusRPCClient {
        private:
        std::shared_ptr<grpc::Channel> ch_status_;
        std::unique_ptr<status::StatusService::Stub> status_;
        public:
        // ctor
        // 建立与远端的RPC连接
        explicit StatusRPCClient(const std::string& status_addr) {
            ch_status_ = grpc::CreateChannel(status_addr, grpc::InsecureChannelCredentials());
        }

        // dtor
        ~StatusRPCClient() = default;

        // @warning Client对象持有该对象的生命周期，不要尝试delete返回的指针
        // @warning Stub对象不是线程安全的
        status::StatusService::Stub* GetStatusStub() {
            if (ch_status_) {
                if (!status_) {
                    status_ = std::make_unique<status::StatusService::Stub>(ch_status_);
                }
                return status_.get();
            }
            return nullptr;
        }
    };
}

#endif