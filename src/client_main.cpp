#include <iostream>

#include <boost/asio.hpp>

#include "session.hpp"
#include "util_class.hpp"
#include "msgnode.hpp"

using namespace boost::asio;
using namespace std;

// TODO(user): Recv还是有问题……接收时只能接收一个报文
// 同步的客户端Session
class ClientSession : public Noncopyable {
public:
    ClientSession(io_context& ctx) : sock_(ctx) {}

    // 同步连接的方法
    bool Connect(ip::tcp::endpoint remote) {
        sock_.open(ip::tcp::v4());
        errcode err;
        sock_.connect(remote, err);
        if (err) {
            printf("Error on connect: %s\n", err.what().c_str());
            return false;
        }
        return true;
    }
    bool Send(const char* msg, uint32_t msg_len, uint32_t tag) {
        send_msg_.Zero();
        // 构造消息节点部分
        send_msg_.Reallocate(HEAD_LEN + msg_len);   // 确保大小足够
        send_msg_.SetTagField(tag);
        send_msg_.SetContentLenField(msg_len);
        memcpy(send_msg_.data_ + HEAD_LEN, msg, msg_len);
        
        // 发送
        while (send_msg_.cur_pos_ < HEAD_LEN + send_msg_.ctx_len_) {
            send_msg_.cur_pos_ += sock_.send(buffer(send_msg_.data_, HEAD_LEN + send_msg_.ctx_len_));
        }
        printf("%u data sent\n", send_msg_.cur_pos_);
        // Send done
        return true;
    }
    bool Receive(MsgNode& msg_node) {
        if (msg_node.max_len_ < HEAD_LEN) {
            return false;
        }
        msg_node.Zero();
        // 接收起始部分
        size_t bytes_read = boost::asio::read(sock_, buffer(msg_node.data_, HEAD_LEN));
        if (bytes_read < HEAD_LEN) {
            return false; // 读取失败
        }
        // 此时已经读取了整个头部
        uint32_t content_len = msg_node.GetContentLenField();
        if (msg_node.max_len_ < HEAD_LEN + content_len) {
            msg_node.Reallocate(HEAD_LEN + content_len);
        }
        bytes_read = boost::asio::read(sock_, buffer(msg_node.data_ + HEAD_LEN, content_len));
        if (bytes_read < content_len) {
            return false;
        }
        return true;
    }

    void Close() {
        sock_.close();
    }
    MsgNode send_msg_{1024};
    MsgNode recv_msg_{1024};
private:
    boost::asio::ip::tcp::socket sock_;
};

// 客户端处要异步对方发送的数据，以及将标准输入的数据异步地发送给服务端

int main() {
    // try {
        boost::asio::io_context ctx;
        ClientSession cs(ctx);        
        // 指定远程端口
        printf("Connecting\n");
        ip::tcp::endpoint remote(ip::tcp::v4(), 1234);
        bool ret = cs.Connect(remote);
        if (!ret) {
            printf("Error in connect\n");
            return -1;
        }
        printf("Connected\n");
        std::string inp;
        while (true) {
            cin >> inp;
            if (inp == "exit") break;
            cs.Send(inp.c_str(), inp.size(), 0);
            cs.Receive(cs.recv_msg_);
            int content_len = cs.recv_msg_.GetContentLenField();
            string msg = string(cs.recv_msg_.GetContent(), cs.recv_msg_.GetContent() + content_len);
            cout << "Received " << msg << endl;
        }
        printf("Closing\n");
        cs.Close();
    // } catch (std::exception& e) {
    //     cerr << "Exception: " << e.what() << endl;
    // }
}