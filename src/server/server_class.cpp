#include <cstdio>
#include <memory>

#ifdef USING_IOCONTEXT_POOL
#include "server/io_context_pool.hpp"
#elif defined(USING_IOTHREAD_POOL)
#include "server/io_thread_pool.hpp"
#endif

#include "server/server_class.hpp"
#include "server/session.hpp"

using namespace boost::asio;
using namespace std;
// async_accept使用
void Server::StartAccept() {
#ifdef USING_IOCONTEXT_POOL
    io_context& ctx = Singleton<IOContextPool>::GetInstance().GetNextIOContext();
#elif defined(USING_IOTHREAD_POOL)
    io_context& ctx = Singleton<IOThreadPool>::GetInstance().GetNextIOContext();
#else
    io_context& ctx = context_;
#endif

    shared_ptr<Session> sess = make_shared<Session>(ctx, this);
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