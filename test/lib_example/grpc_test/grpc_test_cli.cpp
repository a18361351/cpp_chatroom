#include <grpcpp/security/credentials.h>
#include <string>
#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "demo.grpc.pb.h"
#include "demo.pb.h"

using grpc::ClientContext;
using grpc::Channel;
using grpc::Status;

using hello::Greeter;
using hello::HelloReq;
using hello::HelloResp;

class Client {
    public:
    Client(std::shared_ptr<Channel> channel) : stub_(Greeter::NewStub(channel)) {}
    std::string SayHello(std::string msg) {
        ClientContext ctx;
        HelloResp resp;
        HelloReq req;
        req.set_message(msg);

        Status status = stub_->SayHello(&ctx, req, &resp);
        if (status.ok()) {
            return resp.message();
        } else {
            return "error " + status.error_message();
        }
    }
    private:
    std::unique_ptr<Greeter::Stub> stub_;
};

int main() {
    auto channel = grpc::CreateChannel("127.0.0.1:12345", grpc::InsecureChannelCredentials());
    Client cli(channel);
    std::string res = cli.SayHello("grpc test!");
    std::cout << "Result: " << res << std::endl;
    return 0;
}
