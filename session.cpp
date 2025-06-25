#include "session.hpp"
#include "server_class.hpp"

using namespace boost::asio;

void Session::Receiver() {
    auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
        if (err) {
            // tell that error!
            return;
        }
        if (bytes_rcvd == 0) {
            // closing
            self->sock_.close();
            return;
        }
        
        // cur_pos更新
        self->recv_buf_.cur_pos_ += bytes_rcvd;

        if (self->recv_buf_.cur_pos_ >= self->recv_buf_.ctx_len_ + HEAD_LEN) {
            // 处理了一整个消息
            uint32_t tag;

            memcpy(&tag, self->recv_buf_.data_, TAG_LEN);
            tag = ntohl(tag);
            self->ReceiveHandler(self->recv_buf_.ctx_len_, tag);
            // 处理完毕后清除缓冲区，准备下一次接收
            self->recv_buf_.Zero();
        } else if (self->recv_buf_.cur_pos_ >= HEAD_LEN) {
            // 处理了报文长度部分
            // 此时我们可以将ctx_len部分来获取出来了

            memcpy(&self->recv_buf_.ctx_len_, self->recv_buf_.data_ + TAG_LEN, LENGTH_LEN);
            self->recv_buf_.ctx_len_ = ntohl(self->recv_buf_.ctx_len_);

            if (self->recv_buf_.ctx_len_ > MAX_CTX_LEN) {
                // 超出最大报文长度了！
                self->sock_.close();
                return;
            }
            if (self->recv_buf_.ctx_len_ + HEAD_LEN > self->recv_buf_.max_len_) {
                self->recv_buf_.Reallocate(self->recv_buf_.ctx_len_ + HEAD_LEN);
            }
        } 
        self->Receiver();
    };
    // 规范消息头部
    if (recv_buf_.cur_pos_ < HEAD_LEN) {
        boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, HEAD_LEN - recv_buf_.cur_pos_), cb);
    } else {
        boost::asio::async_read(sock_, boost::asio::buffer(recv_buf_.data_ + recv_buf_.cur_pos_, recv_buf_.ctx_len_ + HEAD_LEN - recv_buf_.cur_pos_), cb);
    }
}


// void Session::DoRecv() {
//     // 由于异步回调捕获了self(shared_from_this)，回调完成之前Session对象不会被销毁
//     auto self = shared_from_this();
//     sock_.async_receive(buffer(buf_), [self](const errcode& err, size_t len) {
//         if (!err) {
//             // recv logic
//             self->DoSend();
//         } else {
//             self->srv_->RemoveSession(self->uuid_);
//         }
//     });
// }

// void Session::DoSend() {
//     auto self = shared_from_this();
//     sock_.async_send(buffer(buf_), [self](const errcode& err, size_t len) {
//         if (!err) {
//             // send logic
//             self->DoRecv();
//         } else {
//             self->srv_->RemoveSession(self->uuid_);
//         }
//     });
// }

void Session::Send(const char* msg, int send_len) {
    msg_ptr ptr = std::make_shared<MsgNode>(msg, send_len);
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