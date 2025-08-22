#include "common/timer.hpp"

namespace chatroom {
TimedTask::TimedTask(TimerTaskManager &mgr) : timer_(mgr.GetContext()) {}

void TimedTask::SetTimer(std::chrono::milliseconds duration, std::function<void()> callback, bool auto_reset) {
    dur_ = duration;
    cb_ = std::move(callback);
    auto_reset_ = auto_reset;
}

void TimedTask::Activate() {
    timer_.expires_after(dur_);
    active_ = true;
    timer_.async_wait([self = shared_from_this()](const boost::system::error_code &ec) {
        if (!ec && self->active_) {
            self->cb_();
            if (self->auto_reset_) {
                self->Activate();
            }
        }
    });
}

void TimedTask::Cancel() {
    auto_reset_ = false;
    active_ = false;
    timer_.cancel();
}

using TaskIter = std::list<std::shared_ptr<TimedTask>>::iterator;

TaskIter TimerTaskManager::CreateTimer(std::chrono::milliseconds duration, std::function<void()> callback,
                                       bool auto_reset) {
    std::unique_lock lock(lck_);
    list_.push_back(std::make_shared<TimedTask>(*this));
    auto timer = list_.back();
    timer->SetTimer(duration, std::move(callback), auto_reset);
    return --list_.end();
}

void TimerTaskManager::RemoveTimer(TaskIter iter) {
    std::unique_lock lock(lck_);
    if (iter != list_.end()) {
        (*iter)->Cancel();
        list_.erase(iter);
    }
}

boost::asio::io_context &TimerTaskManager::GetContext() { return ctx_; }

// @brief 创建一个工作线程执行定时任务
void TimerTaskManager::StartWorker() {
    th_ = std::thread([this]() { ctx_.run(); });
}

// @brief 停止所有定时器，以及工作线程；未执行的任务会被立即中断
void TimerTaskManager::StopWorker() {
    if (th_.joinable()) {
        work_guard_.reset();
        ctx_.stop();
        th_.join();
    }
}

}  // namespace chatroom
