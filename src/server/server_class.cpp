#include <cstdio>
#include <memory>

#include "log/log_manager.hpp"
#include "server/server_class.hpp"
#include "server/session.hpp"


namespace chatroom::backend {
    void ServerClass::AcceptorFn() {
        boost::asio::io_context& ctx = this->ctx_;
        std::shared_ptr<Session> sess = make_shared<Session>(ctx, handler_, sess_mgr_);
        auto accept_token = [this, sess](const errcode& err) {
            if (!err) {
                // accept new session
                sess_mgr_->AddTempSession(sess);
                spdlog::info("New session incoming: {}", sess->sock_.remote_endpoint().address().to_string());
                
                // 启动Session的运行
                sess->Start();
                AcceptorFn();
            } else {
                spdlog::error("Error when accepting new session: {}", err.what());
                return;
            }
        };
        acc_.async_accept(sess->sock_, accept_token);
    }
    
}