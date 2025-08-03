#include "http/gateway_class.hpp"

using namespace std;

namespace asio = boost::asio;
namespace mysql = boost::mysql;

void chatroom::gateway::GatewayClass::Run(uint db_conn) {
    spdlog::info("Gateway APP start running");
    bool ret = dbm_->Start(db_conn, db_conn);
    if (!ret) {
        throw std::runtime_error("Error when starting dbm");
    }
    http_->Start();
}
