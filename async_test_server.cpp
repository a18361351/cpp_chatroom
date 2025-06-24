#include <iostream>
using namespace std;

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace boost::asio;

using errcode = boost::system::error_code;

class Server;

const int BUF_SIZE = 1024;

class Session : public enable_shared_from_this<Session> {
public:
    friend class Server;
    Session(ip::tcp::socket sock, Server* srv) : sock_(std::move(sock)), srv_(srv), buf_(BUF_SIZE) {
        // 创建UUID
        boost::uuids::uuid u = boost::uuids::random_generator()();
        uuid_ = boost::uuids::to_string(u);
    }
    const string& uuid() const {
        return uuid_;
    }

    // 已建立链接的Sess开始执行
    // 注意Session的生命周期管理由自己以及Server的sessions集合对象管理
    void Start() {
        DoRecv();
    }

    void DoRecv();
    void DoSend();

private:
    std::string uuid_;
    ip::tcp::socket sock_;
    Server* srv_;
    vector<char> buf_;
};

class Server {
public:
    friend class Session;
    Server(io_context& context) : context_(context), acc_(context) {}

    void Listen(ip::tcp::endpoint ep) {
        acc_.bind(ep);
        acc_.listen();  // default backlog
    }

    void StartAccept() {
        // 3 of 8 overloads，接受errcode以及已经接受了的socket对象（的移动）
        // 这也是所谓的move accept token
        // move accept token: [https://www.boost.org/doc/libs/latest/doc/html/boost_asio/reference/MoveAcceptToken.html]
        acc_.async_accept([this](const errcode& err, ip::tcp::socket peer) -> void {
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
    
    // 强制下线逻辑
    bool Kick(const string& uuid) {
        auto iter = sessions_.find(uuid);
        if (iter == sessions_.end()) return false;

        iter->second->sock_.close();    // 关闭其上所有连接！
        return true;
    }

    
private:

    bool RemoveSession(const string& uuid) {
        auto iter = sessions_.find(uuid);
        if (iter == sessions_.end()) return false;
        sessions_.erase(iter);
        return true;
    }

    bool running{true};
    // 通过sessions_储存这些会话的智能指针，保持其不被销毁
    unordered_map<string, shared_ptr<Session>> sessions_;
    io_context& context_;
    ip::tcp::acceptor acc_;
};


void Session::DoRecv() {
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


int main() {
    io_context context;
    Server s1(context);

    ip::tcp::endpoint ep(ip::tcp::v4(), 1234);

    s1.Listen(ep);
    s1.StartAccept();

    context.run();
}