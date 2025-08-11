#ifndef COMMON_TIMER_HEADER
#define COMMON_TIMER_HEADER

#include <atomic>
#include <chrono>
#include <list>
#include <memory>
#include <thread>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

namespace chatroom {
    class TimerTaskManager;
    
    class TimedTask : public std::enable_shared_from_this<TimedTask>{
        public:
        TimedTask(TimerTaskManager& mgr);
    
        void SetTimer(std::chrono::milliseconds duration, std::function<void()> callback, bool auto_reset = true);
    
        void Activate();
    
        // @brief 取消定时操作，Cancel()调用后，回调不会被执行
        void Cancel();
        
        private:
        boost::asio::steady_timer timer_;
        std::function<void()> cb_;
        std::atomic_bool auto_reset_;
        std::atomic_bool active_{false};
        std::chrono::milliseconds dur_;
    };
    
    class TimerTaskManager {
        public:
        using TaskIter = std::list<std::shared_ptr<TimedTask>>::iterator;
    
        TimerTaskManager() {
            StartWorker();
        }
    
        ~TimerTaskManager() {
            StopWorker();
        }
        
        TaskIter CreateTimer(std::chrono::milliseconds duration, std::function<void()> callback, bool auto_reset = true);
    
        void RemoveTimer(TaskIter iter);
    
        boost::asio::io_context& GetContext();
        private:
        // @brief 创建一个工作线程执行定时任务
        void StartWorker();
    
        // @brief 停止所有定时器，以及工作线程；未执行的任务会被立即中断
        void StopWorker();

        std::list<std::shared_ptr<TimedTask>> list_;
        boost::asio::io_context ctx_;
        std::thread th_;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_{ctx_.get_executor()};
        std::mutex lck_;
    };
}

#endif