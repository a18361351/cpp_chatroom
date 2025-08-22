#include <iostream>
#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <jsoncpp/json/json.h>

#include "client/client_class.hpp"
#include "common/msgnode.hpp"
#include "log/log_manager.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ip = asio::ip;
using namespace std;

namespace chatroom::client {
    bool GatewayHelper::Login(const string& username, const string& passcode, string& token, string& server_addr, uint64_t& uid) {
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
            if (resp.result() == http::status::conflict) {
                // 409 conflict
                // 表示已有其他客户端登录，等待一定时间后重试
                int attempt = 0;
                vector<int> wait_time = {1, 1, 2, 3};
                bool done = false;
                while (attempt < 4) {
                    tcp.close();
                    resp = http::response<http::dynamic_body>();
                    recv_buf.clear();
                    sleep(wait_time[attempt]);
                    tcp.connect(addr_result);
                    http::write(tcp, req);
                    http::read(tcp, recv_buf, resp);
                    if (resp.result() == http::status::ok) {
                        done = true;
                        break;
                    } else if (resp.result() == http::status::conflict) {
                        // 继续重试
                        attempt++;
                        continue;
                    } else {
                        // 其他错误
                        break;
                    }
                }
                if (!done) {
                    std::string resp_body = beast::buffers_to_string(resp.body().data());
                    spdlog::error("Login failed: {}", resp_body);
                    return false;
                }
            } else {
                // other error
                std::string resp_body = beast::buffers_to_string(resp.body().data());
                spdlog::error("Login failed: {}", resp_body);
                return false;
            }
        }

        // 登录成功获取其中的token和服务器地址进行登录
        std::string resp_body = beast::buffers_to_string(resp.body().data());
        Json::Reader rr;
        Json::Value readed;
        bool ret = rr.parse(resp_body, readed, false);

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
    bool GatewayHelper::Register(const string& username, const string& passcode) {
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
        req.prepare_payload();

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
    void GatewayHelper::SetRemoteAddress(const string& host, const string& port) {
        host_ = host;
        port_ = port;
    }

    // AsyncClient
    void AsyncClient::Start(const ip::tcp::endpoint& remote) {
        sock_.open(ip::tcp::v4());
        sock_.async_connect(remote, [self = shared_from_this()](const boost::system::error_code& err) {
            self->ConnectedHandler();
            self->Receiver();
        });
        ctx_.run();
    }
    void AsyncClient::ConnectedHandler() {
        spdlog::info("Connected");
        running_ = true;
        worker_ = std::thread([self = shared_from_this()]() {
            self->WorkerFn();
        });
    }
    void AsyncClient::Receiver() {
        if (recv_buf_.cur_pos_ < HEAD_LEN) {
            ReceiveHead();
        } else {
            ReceiveContent();
        }
    }

    void AsyncClient::ReceiveHead() {
        auto cb = [self = shared_from_this()](const boost::system::error_code& err, std::size_t bytes_rcvd) {
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
        boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, HEAD_LEN - recv_buf_.cur_pos_), 
                                boost::asio::bind_executor(strand_, cb));
    }

    void AsyncClient::ReceiveContent() {
        auto cb = [self = shared_from_this()](const boost::system::error_code& err, std::size_t bytes_rcvd) {
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
        boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.ctx_len_ + HEAD_LEN - recv_buf_.cur_pos_), 
                                boost::asio::bind_executor(strand_, cb));
    }

    // 通过字符串和最大长度构造带有消息的节点对象，函数內部会进行消息的拷贝，同时根据msg_len和tag的值设定头部
    void AsyncClient::Send(const char* msg, uint32_t msg_len, uint32_t tag) {
        std::shared_ptr<MsgNode> send_msg = make_shared<MsgNode>(msg, msg_len, tag);
        
        unique_lock lock(mtx_);
        send_q_.push_back(std::move(send_msg));
        if (send_q_.size() == 1) {
            // 唤醒工作线程
            cv_.notify_one();
        }
    }

    void AsyncClient::ReceiveHandler(uint32_t len, uint32_t tag) {
        string content = std::string(recv_buf_.data_ + HEAD_LEN, recv_buf_.ctx_len_);
        spdlog::debug("Received {} data: {}", TagTypeStr((TagType)tag), content);
        
        switch (tag) {
            case VERIFY_DONE:
            cout << "Verify done: " << content << '\n';
            break;
            case CHAT_MSG_TOCLI:
            {
                uint64_t from_uid = ReadNetField64(recv_buf_.GetContent());
                content = content.substr(sizeof(uint64_t));
                cout << "From UID:" << from_uid << '\n';
                cout << content << '\n';
            }
            break;
            default:
            spdlog::warn("Client received unknown message type: {}", TagTypeStr((TagType)tag));
        }
    }

    void AsyncClient::Close() {
        if (running_.exchange(false)) {
            sock_.close();
            cv_.notify_all();   // 让worker线程能够退出
        }
    }

    void AsyncClient::WorkerFn() {
        while (running_) {
            unique_lock lock(mtx_);
            while (running_ && send_q_.empty()) {
                cv_.wait(lock);
                continue;
            }
            if (!running_) break;
            sending_ = std::move(send_q_.front());
            send_q_.pop_front();
            lock.unlock();
            boost::system::error_code ec;
            boost::asio::write(sock_, asio::buffer(sending_->data_, sending_->ctx_len_ + HEAD_LEN), ec);
            if (ec) {
                spdlog::error("Error occurred while sending data: {}", ec.message());
                Close();
                return;
            }
            spdlog::debug("Sent {} bytes of content", sending_->ctx_len_);
        }
        spdlog::info("Worker thread exited");
    }

    void AsyncClient::Verify(uint64_t uid, const string& token) {
        // 发送验证请求
        Json::Value root;
        root["uid"] = uid;
        root["token"] = token;

        Json::FastWriter fw;
        string msg = fw.write(root);

        Send(msg.c_str(), msg.size(), VERIFY);
    }

    void AsyncClient::WorkerJoin() {
        worker_.join();
    }
    bool AsyncClient::Running() const {
        return running_;
    }
}