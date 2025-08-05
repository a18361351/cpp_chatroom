#include <cstdio>
#include <memory>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/write.hpp>

#include "log/log_manager.hpp"
#include "server/msg_handler.hpp"
#include "server/session.hpp"
#include "server/server_class.hpp"

using namespace std;
using namespace boost::asio;

void chatroom::backend::Session::Send(const char* content, uint32_t send_len, uint32_t tag) {
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
void chatroom::backend::Session::Send(shared_ptr<MsgNode> ptr) {
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

void chatroom::backend::Session::QueueSend()  {
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

        bool continue_flag = false;
        // 临界区
        {
            locker lck(self->send_latch_);
            
            msg_ptr ptr = self->send_q_.front();
            ptr->cur_pos_ += bytes_sent;
            
            //  我们发送的消息没发完的情况（实际上async_write能够确保数据节点被发送完全，此时应该直接免去
            // 发送大小的检查。但为了健壮性暂时留着
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
    // strand保护同一strand的回调不会被并发执行
    boost::asio::async_write(sock_, buffer(front->data_ + front->cur_pos_, front->max_len_ - front->cur_pos_), bind_executor(strand_, cb));
}

void chatroom::backend::Session::ReceiveHandler(uint32_t content_len, uint32_t tag) {
    auto handler = handler_.lock();
    if (!handler) {
        this->down_ = true; // 停止Session的运行
        return; // 异步过程中，handler对象已经被销毁了
    }
    handler->PostMessage(shared_from_this(), std::move(this->recv_ptr_));
    this->recv_ptr_ = make_shared<MsgNode>(INITIAL_NODE_SIZE);  // 发送完数据之后创建新节点
}

void chatroom::backend::Session::ReceiveHead() {
    auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
        if (err) {
            if (err == boost::asio::error::eof) {
                spdlog::debug("EOF when receiving content");
                return;
            }
            // tell that error!
            spdlog::error("Error occured in ReceiveContent(): {}", err.what().c_str());
            return;
        }
        // cur_pos更新
        self->recv_ptr_->cur_pos_ += bytes_rcvd;
        if (self->recv_ptr_->cur_pos_ >= HEAD_LEN) {
            // 处理了报文长度部分
            // 此时我们可以将ctx_len部分来获取出来了
            uint32_t content_len = self->recv_ptr_->UpdateContentLenField();
            if (content_len > MAX_CTX_LEN) {
                // 超出最大报文长度了！
                self->sock_.close();
                return;
            }
            if (content_len + HEAD_LEN > self->recv_ptr_->max_len_) {
                self->recv_ptr_->Reallocate(content_len + HEAD_LEN);
            }
            self->ReceiveContent();
        } else {
            // 继续接收头部
            self->ReceiveHead();
        }
    };

    // 消息头部
    sock_.async_receive(boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, HEAD_LEN - recv_ptr_->cur_pos_), 
                        boost::asio::bind_executor(strand_, cb));
    // boost::asio::async_read(sock_, boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, HEAD_LEN - recv_ptr_->cur_pos_), boost::asio::bind_executor(strand_, cb));
}
void chatroom::backend::Session::ReceiveContent() {
    auto cb = [self = shared_from_this()](const errcode& err, std::size_t bytes_rcvd) {
        if (err) {
            if (err == boost::asio::error::eof) {
                spdlog::debug("EOF when receiving content");
                return;
            }
            // tell that error!
            spdlog::error("Error occured in ReceiveContent(): {}", err.what().c_str());
            return;
        }
        // cur_pos更新
        self->recv_ptr_->cur_pos_ += bytes_rcvd;
        if (self->recv_ptr_->cur_pos_ >= self->recv_ptr_->ctx_len_ + HEAD_LEN) {
            // 处理了一整个消息
            uint32_t tag = self->recv_ptr_->GetTagField();
            self->ReceiveHandler(self->recv_ptr_->ctx_len_, tag);
            // 处理完毕后清除缓冲区，准备下一次接收
            self->recv_ptr_->Zero();
            self->ReceiveHead();
        } else {
            self->ReceiveContent();
        }
    };
    // 消息体
    sock_.async_receive(boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, recv_ptr_->ctx_len_ + HEAD_LEN - recv_ptr_->cur_pos_), 
                        boost::asio::bind_executor(strand_, cb));
    // boost::asio::async_read(sock_, boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, recv_ptr_->ctx_len_ + HEAD_LEN - recv_ptr_->cur_pos_), boost::asio::bind_executor(strand_, cb));
}