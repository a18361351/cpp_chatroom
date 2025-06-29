#include <iostream>

#include <boost/asio.hpp>
#include <memory>

#include "utils/util_class.hpp"
#include "common/msgnode.hpp"

using errcode = boost::system::error_code;

using namespace boost::asio;
using namespace std;

// TODO(user): 为什么加上Noncopyable就无法share_from_this?
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
        printf("Connected\n");
        fflush(stdout);
        Send("Hello!", 6, HELLO_MSG);
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
            fprintf(stdout, "Receiver() received %lu bytes\n", bytes_rcvd);
            fflush(stdout);
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
                fflush(stdout);
                return;
            }
            printf("%zu bytes sent\n", n);
        };

        async_write(sock_, buffer(send_msg->data_, send_msg->max_len_), cb);
        // Send done
        return true;
    }

    void ReceiveHandler(uint32_t len, uint32_t tag) {
        printf("Length: %u\nTag: %u\n", len, tag);
        fwrite(recv_buf_.data_ + HEAD_LEN, 1, len, stdout);
        printf("\n");
        fflush(stdout);
    }

    void Close() {
        sock_.close();
    }
    MsgNode recv_buf_{1024};
private:
    boost::asio::ip::tcp::socket sock_;
    io_context& ctx_;
};


int main() {
    // try {
        boost::asio::io_context ctx;
        shared_ptr<AsyncClientSess> sess = make_shared<AsyncClientSess>(ctx);
        assert(sess);
        // 指定远程端口
        printf("Connecting\n");
        ip::tcp::endpoint remote(ip::tcp::v4(), 1234);
        // 使用异步线程来接收消息，简单处理
        std::thread receiver([sess, remote]() {
            sess->Start(remote);
        });

        std::string inp;
        while (true) {
            cin >> inp;
            if (inp == "exit") break;
            sess->Send(inp.c_str(), inp.size(), TEXT_MSG);
        }
        printf("Closing\n");
        sess->Close();
        receiver.join();
    // } catch (std::exception& e) {
    //     cerr << "Exception: " << e.what() << endl;
    // }
}

// 同步的客户端Session
// class ClientSession : public Noncopyable {
// public:
//     ClientSession(io_context& ctx) : sock_(ctx) {}

//     // 同步连接的方法
//     bool Connect(ip::tcp::endpoint remote) {
//         sock_.open(ip::tcp::v4());
//         errcode err;
//         sock_.connect(remote, err);
//         if (err) {
//             printf("Error on connect: %s\n", err.what().c_str());
//             return false;
//         }
//         return true;
//     }
//     bool Send(const char* msg, uint32_t msg_len, uint32_t tag) {
//         send_msg_.Zero();
//         // 构造消息节点部分
//         send_msg_.Reallocate(HEAD_LEN + msg_len);   // 确保大小足够
//         send_msg_.SetTagField(tag);
//         send_msg_.SetContentLenField(msg_len);
//         memcpy(send_msg_.data_ + HEAD_LEN, msg, msg_len);
        
//         // 发送
//         while (send_msg_.cur_pos_ < HEAD_LEN + send_msg_.ctx_len_) {
//             send_msg_.cur_pos_ += sock_.send(buffer(send_msg_.data_, HEAD_LEN + send_msg_.ctx_len_));
//         }
//         printf("%u data sent\n", send_msg_.cur_pos_);
//         // Send done
//         return true;
//     }
//     bool Receive(MsgNode& msg_node) {
//         if (msg_node.max_len_ < HEAD_LEN) {
//             return false;
//         }
//         msg_node.Zero();
//         // 接收起始部分
//         size_t bytes_read = boost::asio::read(sock_, buffer(msg_node.data_, HEAD_LEN));
//         if (bytes_read < HEAD_LEN) {
//             return false; // 读取失败
//         }
//         // 此时已经读取了整个头部
//         uint32_t content_len = msg_node.GetContentLenField();
//         if (msg_node.max_len_ < HEAD_LEN + content_len) {
//             msg_node.Reallocate(HEAD_LEN + content_len);
//         }
//         bytes_read = boost::asio::read(sock_, buffer(msg_node.data_ + HEAD_LEN, content_len));
//         if (bytes_read < content_len) {
//             return false;
//         }
//         return true;
//     }

//     void Close() {
//         sock_.close();
//     }
//     MsgNode send_msg_{1024};
//     MsgNode recv_msg_{1024};
// private:
//     boost::asio::ip::tcp::socket sock_;
// };



// int main() {
//     // try {
//         boost::asio::io_context ctx;
//         ClientSession cs(ctx);        
//         // 指定远程端口
//         printf("Connecting\n");
//         ip::tcp::endpoint remote(ip::tcp::v4(), 1234);
//         bool ret = cs.Connect(remote);
//         if (!ret) {
//             printf("Error in connect\n");
//             return -1;
//         }
//         printf("Connected\n");

//         // 使用异步线程来接收消息，简单处理
//         std::thread receiver([]() {

//         });

//         cs.Send("Hello!", 6, HELLO_MSG);
//         std::string inp;
//         while (true) {
//             cin >> inp;
//             if (inp == "exit") break;
//             cs.Send(inp.c_str(), inp.size(), 0);
//             cs.Receive(cs.recv_msg_);
//             int content_len = cs.recv_msg_.GetContentLenField();
//             string msg = string(cs.recv_msg_.GetContent(), cs.recv_msg_.GetContent() + content_len);
//             cout << "Received " << msg << endl;
//         }
//         printf("Closing\n");
//         cs.Close();
//     // } catch (std::exception& e) {
//     //     cerr << "Exception: " << e.what() << endl;
//     // }
// }