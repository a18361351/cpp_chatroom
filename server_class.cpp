#include "server_class.hpp"
#include "session.hpp"

using namespace boost::asio;


void Server::StartAccept() {
    // 3 of 8 overloads，接受errcode以及已经接受了的socket对象（的移动）
    // 这也是所谓的move accept token
    // move accept token: [https://www.boost.org/doc/libs/latest/doc/html/boost_asio/reference/MoveAcceptToken.html]
    acc_.async_accept([this](const errcode& err, boost::asio::ip::tcp::socket peer) -> void {
        if (!err) {
            // accept new session
            auto sess = std::make_shared<Session>(peer, this);
            sessions_[sess->uuid()] = sess;
            sess->Start();

            StartAccept();
        } else {
            // TODO(user): tell the error

        }
    });
}

bool Server::Kick(const std::string& uuid) {
    auto iter = sessions_.find(uuid);
    if (iter == sessions_.end()) return false;

    iter->second->down_ = true;     // 下线该session
    iter->second->sock_.close();    // 关闭其对应连接！
    // 让Session类的回调来销毁
    return true;
}

bool Server::RemoveSession(const std::string& uuid)  {
    auto iter = sessions_.find(uuid);
    if (iter == sessions_.end()) return false;
    sessions_.erase(iter);
    return true;
}