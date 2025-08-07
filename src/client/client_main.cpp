#include <iostream>
#include <memory>
#include <string>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

#include "common/msgnode.hpp"
#include "log/log_manager.hpp"
#include "utils/util_class.hpp"

using errcode = boost::system::error_code;

using namespace boost::asio;
using namespace std;
using namespace chatroom;
namespace beast = boost::beast;
namespace http = beast::http;

// Synced
// 其登录和注册接口是同步的
class GatewayHelper {
    public:
    GatewayHelper(io_context& ctx) : ctx_(ctx) {}
    bool Login(const string& username, const string& passcode, string& token, string& server_addr, uint64_t& uid) {
        ip::tcp::resolver resl(ctx_);
        // look up domain name
        auto const addr_result = resl.resolve(host_, port_);
        beast::tcp_stream tcp(ctx_);        
        tcp.connect(addr_result);

        // 连接后，发送一个登录请求
        http::request<http::string_body> req{http::verb::post, "/login", 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "ChatClient/1.0");
        req.set(http::field::content_type, "application/json");

        Json::Value root;
        root["username"] = username;
        root["passcode"] = passcode;
        Json::FastWriter fw;

        req.body() = fw.write(root);
        req.prepare_payload();

        // 进行发送
        http::write(tcp, req);

        beast::flat_buffer recv_buf;
        http::response<http::dynamic_body> resp;

        // 接收响应
        http::read(tcp, recv_buf, resp);
        if (resp.result() != http::status::ok) {
            std::string resp_body = beast::buffers_to_string(resp.body().data());
            spdlog::error("Login failed: {}", resp_body);
            return false;
        }

        // 登录成功获取其中的token和服务器地址进行登录
        Json::Reader rr;
        Json::Value readed;
        bool ret = rr.parse(beast::buffers_to_string(resp.body().data()), readed, false);

        // http_gate处代码
        // resp_json["token"] = token;
        // resp_json["server_addr"] = addr;
        if (!ret) {
            spdlog::error("Error occured when parsing json");
            return false;
        }
        try {
            token = readed["token"].asString();
            server_addr = readed["server_addr"].asString();
            uid = readed["uid"].asUInt64();
        } catch (...) {
            spdlog::error("Error occured when parsing json");
            return false;
        }
        return true;
    }
    bool Register(const string& username, const string& passcode) {
        ip::tcp::resolver resl(ctx_);
        // look up domain name
        auto const addr_result = resl.resolve(host_, port_);
        beast::tcp_stream tcp(ctx_);        
        tcp.connect(addr_result);

        // 连接后，发送一个登录请求
        http::request<http::string_body> req{http::verb::post, "/register", 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "ChatClient/1.0");
        req.set(http::field::content_type, "application/json");

        Json::Value root;
        root["username"] = username;
        root["passcode"] = passcode;
        Json::FastWriter fw;

        req.body() = fw.write(root);

        // 进行发送
        http::write(tcp, req);

        beast::flat_buffer recv_buf;
        http::response<http::dynamic_body> resp;

        // 接收响应
        http::read(tcp, recv_buf, resp);
        if (resp.result() != http::status::ok) {
            std::string resp_body = beast::buffers_to_string(resp.body().data());
            spdlog::error("Register failed: {}", resp_body);
            return false;
        }
        return true;
    }
    void SetRemoteAddress(const string& host, const string& port) {
        host_ = host;
        port_ = port;
    }
    private:
    io_context& ctx_;
    string host_;
    string port_;
};


class AsyncClient : public enable_shared_from_this<AsyncClient> {
public:
    explicit AsyncClient(io_context& ctx) : sock_(ctx), ctx_(ctx) {}
    // 启动连接
    void Start(const ip::tcp::endpoint& remote) {
        sock_.open(ip::tcp::v4());
        sock_.async_connect(remote, [self = shared_from_this()](const errcode& err) {
            self->ConnectedHandler();
            self->Receiver();
        });
        ctx_.run();
    }
    void ConnectedHandler() {
        spdlog::info("Connected");
        running_ = true;
        worker_ = std::thread([self = shared_from_this()]() {
            self->WorkerFn();
        });
    }
    void Receiver() {
        if (recv_buf_.cur_pos_ < HEAD_LEN) {
            ReceiveHead();
        } else {
            ReceiveContent();
        }
    }

    void ReceiveHead() {
        auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
            if (err) {
                // tell that error!
                spdlog::error("Error when receiving head: {}", err.message());
                self->Close();  // 由于连接错误，我们需要关闭连接
                return;
            }
            spdlog::debug("Received {} bytes of head", bytes_rcvd);
            // cur_pos更新
            self->recv_buf_.cur_pos_ += bytes_rcvd;
            if (self->recv_buf_.cur_pos_ >= HEAD_LEN) {
                // 处理了报文长度部分
                // 此时我们可以将ctx_len部分来获取出来了

                uint32_t content_len = self->recv_buf_.UpdateContentLenField();

                if (content_len > MAX_CTX_LEN) {
                    // 超出最大报文长度了！
                    self->sock_.close();
                    return;
                }
                if (content_len + HEAD_LEN > self->recv_buf_.max_len_) {
                    self->recv_buf_.Reallocate(content_len + HEAD_LEN);
                }
                self->ReceiveContent();
            } else {
                // 继续接收头部
                self->ReceiveHead();
            }
        };
    
        // 消息头部
        // sock_.async_receive(boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, HEAD_LEN - recv_buf_.cur_pos_), cb);
        boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, HEAD_LEN - recv_buf_.cur_pos_), cb);
    }

    void ReceiveContent() {
        auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
            if (err) {
                // tell that error!
                spdlog::error("Error when receiving content: {}", err.message());
                self->Close();
                return;
            }
            spdlog::debug("Received {} bytes of content", bytes_rcvd);
            // cur_pos更新
            self->recv_buf_.cur_pos_ += bytes_rcvd;

            if (self->recv_buf_.cur_pos_ >= self->recv_buf_.ctx_len_ + HEAD_LEN) {
                // 处理了一整个消息
                uint32_t tag = self->recv_buf_.GetTagField();
                self->ReceiveHandler(self->recv_buf_.ctx_len_, tag);
                // 处理完毕后清除缓冲区，准备下一次接收
                self->recv_buf_.Zero();
                self->ReceiveHead();
            } else {
                self->ReceiveContent();
            }
        };
        // 消息体
        // sock_.async_receive(boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.ctx_len_ + HEAD_LEN - recv_buf_.cur_pos_), cb);
        boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.ctx_len_ + HEAD_LEN - recv_buf_.cur_pos_), cb);
    }

    // 通过字符串和最大长度构造带有消息的节点对象，函数內部会进行消息的拷贝，同时根据msg_len和tag的值设定头部
    void Send(const char* msg, uint32_t msg_len, uint32_t tag) {
        std::shared_ptr<MsgNode> send_msg = make_shared<MsgNode>(msg, msg_len, tag);
        
        unique_lock lock(mtx_);
        send_q_.push_back(std::move(send_msg));
        if (send_q_.size() == 1) {
            // 唤醒工作线程
            cv_.notify_one();
        }
    }

    void ReceiveHandler(uint32_t len, uint32_t tag) {
        spdlog::info("Received data: {}", std::string(recv_buf_.GetContent(), recv_buf_.GetContentLen()));
    }

    void Close() {
        if (running_.exchange(false)) {
            sock_.close();
            cv_.notify_all();   // 让worker线程能够退出
        }
    }

    void WorkerFn() {
        while (running_) {
            unique_lock lock(mtx_);
            while (running_ && send_q_.empty()) {
                cv_.wait(lock);
                continue;
            }
            if (!running_) break;
            auto msg = send_q_.front();
            send_q_.pop_front();
            lock.unlock();

            // 发送消息
            boost::asio::async_write(sock_, boost::asio::buffer(msg->data_, msg->max_len_), 
                [self = shared_from_this()](const errcode& err, std::size_t bytes_sent) {
                    if (err) {
                        spdlog::error("Error when sending message: {}", err.message());
                        self->Close();
                        return;
                    }
                    spdlog::debug("Sent {} bytes", bytes_sent);
                });
        }
        spdlog::info("Worker thread exited");
    }

    void Verify(uint64_t uid, const string& token) {
        // 发送验证请求
        Json::Value root;
        root["uid"] = uid;
        root["token"] = token;

        Json::FastWriter fw;
        string msg = fw.write(root);

        Send(msg.c_str(), msg.size(), VERIFY);
    }

    void WorkerJoin() {
        worker_.join();
    }
    bool Running() const {
        return running_;
    }

    MsgNode recv_buf_{1024};
private:
    boost::asio::ip::tcp::socket sock_;
    io_context& ctx_;

    // send queue
    std::deque<std::shared_ptr<MsgNode>> send_q_;
    std::mutex mtx_;
    std::thread worker_;
    std::condition_variable cv_;
    std::atomic_bool running_;
};


int main() {
    boost::asio::io_context ctx;
    GatewayHelper gate(ctx);

    string host, port;
    cout << "host: ";
    cin >> host;
    cout << "port: ";
    cin >> port;

    cout << "Remote host set, ";
    gate.SetRemoteAddress(host, port);
    string token, server_addr;
    uint64_t uid;
    while (true) {
        string inp;
        cout << "Input command>";
        cin >> inp;
        if (inp == "login") {
            string username, passcode;
            cout << "Username: ";
            cin >> username;
            cout << "Passcode: ";
            cin >> passcode;
            if (gate.Login(username, passcode, token, server_addr, uid)) {
                cout << "Login success\n";
                cout << "Token: " << token << "\n";
                cout << "Server address: " << server_addr << "\n";
                break;  // 进入下一步，向对应的后台服务器发起登陆请求
            } else {
                cout << "Login failed\n";
            }
        } else if (inp == "register") {
            string username, passcode;
            cout << "Username: ";
            cin >> username;
            cout << "Passcode: ";
            cin >> passcode;
            if (gate.Register(username, passcode)) {
                cout << "Register success\n";
            } else {
                cout << "Register failed\n";
            }
        } else if (inp == "exit") {
            cout << "bye\n";
            return 0;
        } else {
            cout << "Unknown command, available commands: login, register, exit\n";
        }
    }

    // try {
    shared_ptr<AsyncClient> sess = make_shared<AsyncClient>(ctx);
    assert(sess);
    // 指定远程端口
    spdlog::info("Connecting to remote server {}", server_addr);
    // 得到的server_addr是x.x.x.x:xxxx格式
    string server_host, server_port;
    size_t pos = server_addr.find(':');
    if (pos != string::npos) {
        server_host = server_addr.substr(0, pos);
        server_port = server_addr.substr(pos + 1);
    } else {
        spdlog::error("Invalid server address format: {}", server_addr);
        return -1;
    }

    ip::tcp::endpoint remote(ip::make_address(server_host), stoi(server_port));
    // 使用异步线程来接收消息，简单处理
    std::thread receiver([sess, remote]() {
        sess->Start(remote);
    });
    sess->Verify(uid, token);
    std::string inp;
    while (true) {
        cin >> inp;
        if (!sess->Running()) {
            cout << "Connection closed\n";
            break;
        }
        if (inp == "exit") break;
        std::string buf;
        sess->Send(inp.c_str(), inp.size(), DEBUG);
    }
    printf("Closing\n");
    sess->Close();
    sess->WorkerJoin();
    receiver.join();
    // } catch (std::exception& e) {
    //     cerr << "Exception: " << e.what() << endl;
    // }
}
