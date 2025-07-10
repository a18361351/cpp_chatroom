#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_context.h>
#include <iostream>
#include <grpcpp/grpcpp.h>
#include "demo.grpc.pb.h"
#include "demo.pb.h"

using namespace std;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using hello::Greeter;
using hello::HelloReq;
using hello::HelloResp;

class HelloImpl : public Greeter::Service {
    ::grpc::Status SayHello(::grpc::ServerContext *context, const ::hello::HelloReq *request, ::hello::HelloResp *response) override {
        cout << "Request: " << request->message() << endl;
        string hello = "hello, " + request->message();
        response->set_message(hello);
        return Status::OK;
    }
};

void run() {
    std::string addr("0.0.0.0:12345");
    HelloImpl service;
    ServerBuilder builder;

    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << addr << std::endl;

    server->Wait();

}

int main() {
    run();
    return 0;
}