#include <iostream>

#include <boost/asio.hpp>
#include <memory>

#include "utils/util_class.hpp"
#include "common/msgnode.hpp"

using errcode = boost::system::error_code;

using namespace boost::asio;
using namespace std;

class AsyncClientSess : public /*Noncopyable,*/ enable_shared_from_this<AsyncClientSess> {
public:
    AsyncClientSess(io_context& ctx) : sock_(ctx), ctx_(ctx) {}
    // 启动连接
    void Start(ip::tcp::endpoint remote) {
        sock_.open(ip::tcp::v4());
        sock_.async_connect(remote, [self = shared_from_this()](const errcode& err) {
            self->ConnectedHandler();
            self->Receiver();
        });
        ctx_.run();
    }
    void ConnectedHandler() {
        for (int j = 0; j < target_recv_count; j++) {
            Send("TEST_WOW", 8, ECHO_MSG);   // 要求对方回消息
        }
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
                fprintf(stdout, "Receiver() received an error! %s\n", err.what().c_str());
                fflush(stdout);
                return;
            }
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
        sock_.async_receive(boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, HEAD_LEN - recv_buf_.cur_pos_), cb);
        // boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, HEAD_LEN - recv_buf_.cur_pos_), cb);
    }

    void ReceiveContent() {
        auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
            if (err) {
                // tell that error!
                fprintf(stdout, "Receiver() received an error! %s\n", err.what().c_str());
                fflush(stdout);
                return;
            }
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
        sock_.async_receive(boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.ctx_len_ + HEAD_LEN - recv_buf_.cur_pos_), cb);
        // boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.ctx_len_ + HEAD_LEN - recv_buf_.cur_pos_), cb);
    }

    bool Send(const char* msg, uint32_t msg_len, uint32_t tag) {
        std::shared_ptr<MsgNode> send_msg = make_shared<MsgNode>(msg, msg_len, tag);
        
        auto cb = [send_msg](const errcode& err, size_t n) {
            if (err) {
                printf("Send error\n");
                return;
            }
        };

        async_write(sock_, buffer(send_msg->data_, send_msg->max_len_), cb);
        // Send done
        return true;
    }

    void ReceiveHandler(uint32_t len, uint32_t tag) {
        if (tag == ECHO_MSG) {
            recv_count++;
            if (recv_count >= target_recv_count) {
                printf("Client received %d messages, done.\n", recv_count);
                Close();
            } else {
                // printf("Client received %d messages, waiting for more...\n", recv_count);
            }
        } else {
            printf("Received unknowm msg %u bytes with tag %u\n", len, tag);
        }
        
    }

    void Close() {
        sock_.close();
    }
    MsgNode recv_buf_{1024};
    int target_recv_count{0};
    int recv_count{0};
private:
    boost::asio::ip::tcp::socket sock_;
    io_context& ctx_;
};

#define CLIENT_NUM 100
#define MSG_NUM 1000

int main() {
    try {
        ip::tcp::endpoint remote(ip::tcp::v4(), 1234);
        boost::asio::io_context ctx;
        using Client = shared_ptr<AsyncClientSess>;
        vector<Client> cs;
        vector<thread> ts;
        // Start time
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < CLIENT_NUM; i++) {
            auto cli = make_shared<AsyncClientSess>(ctx);
            cs.push_back(cli);
            cli->target_recv_count = MSG_NUM;
            
            ts.emplace_back([cli, remote]() {
                cli->Start(remote);
            });
        }
        // 指定远程端口
        printf("Client: %d\nMessage per cli: %d\nTesting!\n", CLIENT_NUM, MSG_NUM);
        
        for (auto& t : ts) {
            t.join();
        }
        
        // End time
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        printf("All clients finished in %ld ms\n", duration.count());

    } catch (std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
    return 0;
}
