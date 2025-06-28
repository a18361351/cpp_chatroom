#include <cstdio>
#include <memory>

#include "session.hpp"
#include "server_class.hpp"

using namespace std;
using namespace boost::asio;

void Session::Send(const char* content, uint32_t send_len, uint32_t tag) {
    msg_ptr ptr = std::make_shared<MsgNode>(content, send_len, tag);    // 实际缓冲区长度为HEAD_LEN + send_len
    // 如果队列原本是空的，表示当前没有正在执行的发送
    // 那么我们就执行QueueSend函数以启动异步发送程序
    // 对队列的操作通过send_latch保护
    bool start_send;
    {
        // 临界区
        // Send和Send之间不会冲突，能够确保任意时间只有至多一个Session的QueueSend会被执行
        locker lck(send_latch_);
        start_send = send_q_.empty();
        send_q_.push_back(std::move(ptr));
    }
    if (start_send) {
        QueueSend();
    }
}

// Send3
void Session::Send(shared_ptr<MsgNode> ptr) {
    // 如果队列原本是空的，表示当前没有正在执行的发送
    // 那么我们就执行QueueSend函数以启动异步发送程序
    // 对队列的操作通过send_latch保护
    bool start_send;
    {
        // 临界区
        // Send和Send之间不会冲突，能够确保任意时间只有至多一个Session的QueueSend会被执行
        locker lck(send_latch_);
        start_send = send_q_.empty();
        send_q_.push_back(std::move(ptr));
    }
    if (start_send) {
        QueueSend();
    }
}

void Session::QueueSend()  {
    msg_ptr front;

    // 临界区
    {
        locker lck(send_latch_);
        if (send_q_.empty()) return;
        front = send_q_.front();
    }

    // 发送操作完成后的回调函数
    auto cb = [self = shared_from_this()](const errcode& err, size_t bytes_sent) {
        // callback
        if (err) {
            // tell that error!
            return;
        }

        //  我们发送的消息没发完的情况（实际上async_write能够确保数据节点被发送完全，此时应该直接免去
        // 发送大小的检查。但为了健壮性暂时留着
        bool continue_flag = false;
        // 临界区
        {
            locker lck(self->send_latch_);

            msg_ptr ptr = self->send_q_.front();
            ptr->cur_pos_ += bytes_sent;
            
            if (ptr->cur_pos_ >= ptr->max_len_) {
                self->send_q_.pop_front();
            }
            continue_flag = !self->send_q_.empty();
        }
        // 鉴于消费者只有一个，不用担心在临界区之间的间隙中该队列变空
        if (continue_flag) {
            self->QueueSend();
        }
    };

    // 开始异步操作，ptr以及session自身保证有效
    boost::asio::async_write(sock_, boost::asio::buffer(front->data_ + front->cur_pos_, front->max_len_ - front->cur_pos_), cb);

}

void Session::ReceiveHandler(uint32_t content_len, uint32_t tag) {
    // 将接收到的内容写入stdout
    fwrite(recv_buf_.data_ + HEAD_LEN, 1, recv_buf_.ctx_len_,stdout);
    fflush(stdout);

    // 作为ECHO服务器把数据原封不动地发回去
    shared_ptr<MsgNode> ptr = make_shared<MsgNode>(recv_buf_.data_ + HEAD_LEN, recv_buf_.ctx_len_);
    Send(ptr);
}