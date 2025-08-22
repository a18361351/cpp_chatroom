#include "server/session.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <cstdio>
#include <memory>

#include "log/log_manager.hpp"
#include "server/msg_handler.hpp"

using namespace boost::asio;

namespace chatroom::backend {
using msg_ptr = std::shared_ptr<MsgNode>;
void Session::Send(const char *content, uint32_t send_len, uint32_t tag) {
    msg_ptr ptr = std::make_shared<MsgNode>(content, send_len, tag);  // 实际缓冲区长度为HEAD_LEN + send_len
    // 如果队列原本是空的，表示当前没有正在执行的发送
    // 那么我们就执行QueueSend函数以启动异步发送程序
    // 对队列的操作通过send_latch保护
    bool start_send;
    {
        // 临界区
        // Send和Send之间不会冲突，能够确保任意时间只有至多一个Session的QueueSend会被执行
        std::unique_lock lck(send_latch_);
        start_send = send_q_.empty();
        send_q_.push_back(std::move(ptr));
    }
    if (start_send) {
        QueueSend();
    }
}

// Send3
void Session::Send(std::shared_ptr<MsgNode> ptr) {
    // 如果队列原本是空的，表示当前没有正在执行的发送
    // 那么我们就执行QueueSend函数以启动异步发送程序
    // 对队列的操作通过send_latch保护
    bool start_send;
    {
        // 临界区
        // Send和Send之间不会冲突，能够确保任意时间只有至多一个Session的QueueSend会被执行
        std::unique_lock lck(send_latch_);
        start_send = send_q_.empty();
        send_q_.push_back(std::move(ptr));
    }
    if (start_send) {
        QueueSend();
    }
}

void Session::QueueSend() {
    // 临界区
    {
        std::unique_lock lck(send_latch_);
        if (send_q_.empty()) return;
        sending_ = std::move(send_q_.front());
        send_q_.pop_front();
    }

    // 发送操作完成后的回调函数
    auto cb = [self = shared_from_this()](const boost::system::error_code &err, size_t bytes_sent) {
        // callback
        if (err) {
            // tell that error!
            spdlog::error("Error occured in Session::QueueSend(): {}", err.what());
            self->Close();
            return;
        }
        // 鉴于消费者只有一个，不用担心在临界区之间的间隙中该队列变空
        if (!self->down_) {
            self->QueueSend();
        }
    };

    // 开始异步操作，ptr以及session自身保证有效
    // strand保护同一strand的回调不会被并发执行
    assert(sending_->cur_pos_ == 0);  // sending msg's cur_pos should be zero
    boost::asio::async_write(
        sock_, buffer(sending_->data_ + sending_->cur_pos_, sending_->ctx_len_ + HEAD_LEN - sending_->cur_pos_),
        bind_executor(strand_, cb));
}

void Session::ReceiveHandler(uint32_t content_len, uint32_t tag) {
    auto handler = handler_.lock();
    if (!handler) {
        Close();
        return;  // 异步过程中，handler对象已经被销毁了
    }
    handler->PostMessage(shared_from_this(), std::move(this->recv_ptr_));
    this->recv_ptr_ = std::make_shared<MsgNode>(INITIAL_NODE_SIZE);  // 发送完数据之后创建新节点
}

void Session::ReceiveHead() {
    auto cb = [self = shared_from_this()](const boost::system::error_code &err, std::size_t bytes_rcvd) {
        if (err) {
            if (err == boost::asio::error::eof) {
                spdlog::debug("Remote host closed connection");
                self->Close();
                return;
            }
            // tell that error!
            spdlog::error("Error occured in ReceiveContent(): {}", err.what().c_str());
            self->Close();
            return;
        }
        if (self->down_) {  // 检查会话状态
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
                spdlog::error("Message content length exceed: {}", content_len);
                self->Close();
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
    // sock_.async_receive(boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, HEAD_LEN - recv_ptr_->cur_pos_),
    //                     boost::asio::bind_executor(strand_, cb));
    boost::asio::async_read(sock_,
                            boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, HEAD_LEN - recv_ptr_->cur_pos_),
                            boost::asio::bind_executor(strand_, cb));
}
void Session::ReceiveContent() {
    auto cb = [self = shared_from_this()](const boost::system::error_code &err, std::size_t bytes_rcvd) {
        if (err) {
            if (err == boost::asio::error::eof) {
                spdlog::debug("Remote host closed connection");
                self->Close();
                return;
            }
            // tell that error!
            spdlog::error("Error occured in ReceiveContent(): {}", err.what().c_str());
            self->Close();
            return;
        }
        if (self->down_) {  // 检查会话状态
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
    // sock_.async_receive(boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_, recv_ptr_->ctx_len_ + HEAD_LEN -
    // recv_ptr_->cur_pos_),
    //                     boost::asio::bind_executor(strand_, cb));
    boost::asio::async_read(sock_,
                            boost::asio::buffer(recv_ptr_->data_ + recv_ptr_->cur_pos_,
                                                recv_ptr_->ctx_len_ + HEAD_LEN - recv_ptr_->cur_pos_),
                            boost::asio::bind_executor(strand_, cb));
}

void Session::Close() {
    bool expected = false;
    // 原子操作
    if (down_.compare_exchange_strong(expected, true)) {
        // 异步操作会被中断
        sock_.close();
        auto handler = handler_.lock();
        if (handler) {
            // 告诉handler该会话下线，更新在线状态
            handler->PostMessage(shared_from_this(), nullptr);
        }
        auto mgr = mgr_.lock();
        if (mgr) {
            if (verified_) {
                mgr->RemoveSession(user_id_);
            } else {
                mgr->RemoveTempSession(this);
            }
        }
    }
}

}  // namespace chatroom::backend
