#include <cstdio>
#include <memory>

#include "server/server_class.hpp"
#include "server/session.hpp"

using namespace boost::asio;
using namespace std;

// async_accept使用
void Server::StartAccept() {
    // 3 of 8 overloads，接受errcode以及已经接受了的socket对象（的移动）
    // 这也是所谓的move accept token
    // move accept token: [https://www.boost.org/doc/libs/latest/doc/html/boost_asio/reference/MoveAcceptToken.html]
    shared_ptr<Session> sess = make_shared<Session>(ip::tcp::socket(context_), this);
    auto accept_token = [this, sess](const errcode& err) {
        if (!err) {
            // accept new session
            sessions_[sess->uuid()] = sess;
            
            // TODO(user): 用于Debug的地方在之后移除掉或转为log
            fprintf(stdout, "New session %s\n",
                   sess->uuid().c_str());
            fflush(stdout);

            sess->Start();

            StartAccept();
        } else {
            // TODO(user): tell the error
            fprintf(stdout, "Error accepting new session: %s\n", err.message().c_str());
            fflush(stdout);
            return;
        }
    };
    acc_.async_accept(sess->sock_, accept_token);
}

// MoveAcceptToken使用
// void Server::StartAccept() {
//     // 3 of 8 overloads，接受errcode以及已经接受了的socket对象（的移动）
//     // 这也是所谓的move accept token
//     // move accept token: [https://www.boost.org/doc/libs/latest/doc/html/boost_asio/reference/MoveAcceptToken.html]
//     acc_.async_accept(context_, [this](const errcode& err, boost::asio::ip::tcp::socket peer) -> void {
//         if (!err) {
//             // accept new session
//             auto sess = std::make_shared<Session>(std::move(peer), this);
//             sessions_[sess->uuid()] = sess;
            
//             // TODO(user): 用于Debug的地方在之后移除掉或转为log
//             fprintf(stdout, "New session %s",
//                    sess->uuid().c_str());
//             fflush(stdout);

//             sess->Start();

//             StartAccept();
//         } else {
//             // TODO(user): tell the error
//             fprintf(stdout, "Error accepting new session: %s\n", err.message().c_str());
//             fflush(stdout);
//             return;
//         }
//     });
// }

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