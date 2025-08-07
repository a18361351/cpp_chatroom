#ifndef STATUS_UPLOADER_HEADER
#define STATUS_UPLOADER_HEADER

// status_uploader.hpp: 负责向服务器（Redis）定时上传服务器状态

#include <condition_variable>
#include <mutex>

#include "status/redis/status_redis.hpp"
#include "status/load_balancer.hpp"

namespace chatroom::status {
    class TimedUploader {
        private:
        std::thread worker_;
        std::condition_variable cv_;
        std::mutex mtx_;
        std::atomic_bool running_{false};
        bool pending_{false};   // 是否需要更新
        uint32_t err_count_{0};  
        uint32_t err_count_max_{3};
        
        RedisMgr* redis_;       // 不负责这两对象的生命周期管理，请在销毁这些对象前，先调用Stop()
        LoadBalancer* balancer_;
        uint32_t interval_;     // seconds
        public:

        // ctor
        // @warning 对象不负责RedisMgr和LoadBalancer的生命周期管理，请在销毁这些对象前，先调用Stop()
        TimedUploader(RedisMgr* redis, LoadBalancer* balancer, uint32_t interval_sec = 15) : 
            redis_(redis)
            , balancer_(balancer)
            , interval_(interval_sec) {}
        
        // dtor
        ~TimedUploader() {
            if (running_) {
                Stop();
            }
        }

        // @brief 启动定时上传类，这会异步地启动一个工作线程
        // @warning 请不要重复调用该函数
        void Start();

        // @brief 停止定时上传类的运行，并停止工作线程
        // @warning 请不要重复调用该函数，也不要在Start()之前就调用
        void Stop();

        // @brief 手动执行一次更新
        void UpdateNow();

        private:
        void WorkerFn();
    };

}

#endif