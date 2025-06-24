#include "session.hpp"
#include "server_class.hpp"

using namespace boost::asio;

void Session::DoRecv() {
    // 由于异步回调捕获了self(shared_from_this)，回调完成之前Session对象不会被销毁
    auto self = shared_from_this();
    sock_.async_receive(buffer(buf_), [self](const errcode& err, size_t len) {
        if (!err) {
            // recv logic
            self->DoSend();
        } else {
            self->srv_->RemoveSession(self->uuid_);
        }
    });
}

void Session::DoSend() {
    auto self = shared_from_this();
    sock_.async_send(buffer(buf_), [self](const errcode& err, size_t len) {
        if (!err) {
            // send logic
            self->DoRecv();
        } else {
            self->srv_->RemoveSession(self->uuid_);
        }
    });
}