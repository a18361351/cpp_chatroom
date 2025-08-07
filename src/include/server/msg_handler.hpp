#ifndef SERVER_MESSAGE_HANDLER_HEADER
#define SERVER_MESSAGE_HANDLER_HEADER

// msg_handler.hpp: 消息处理器，其由单个工作线程异步处理这些消息（也许可以扩展为线程池？）

#include <condition_variable>
#include <queue>
#include <cstdint>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <deque>
#include <memory>

#include "common/msgnode.hpp"
#include "server/redis/server_redis.hpp"
#include "server/session_manager.hpp"
#include "utils/util_class.hpp"


namespace chatroom::backend {
    

    using CbSessType = std::shared_ptr<chatroom::backend::Session>;
    using RcvdMsgType = std::shared_ptr<chatroom::MsgNode>; // ReceiveMsg

    // 对于特定tag消息的回调函数，传入参数为Session和MsgNode
    using FuncCallback = 
        std::function<void(CbSessType, RcvdMsgType)>;

    class MsgHandler {
        public:
        MsgHandler(std::shared_ptr<SessionManager> sess_mgr, std::shared_ptr<RedisMgr> redis) : sess_mgr_(std::move(sess_mgr)), redis_(std::move(redis)) {}

        // @brief 异步向处理队列投递一个消息，并进行处理
        bool PostMessage(CbSessType sess, RcvdMsgType msg);

        // @brief 启动消息处理器的工作线程
        bool Start();

        // @brief 关闭消息处理器，停止其工作线程并join
        bool Stop();
        private:
        void Worker();
        void Processor(CbSessType&&, RcvdMsgType&&);
        std::mutex lck_;
        std::condition_variable cv_;
        std::thread worker_;
        std::queue<std::pair<CbSessType, RcvdMsgType>> q_;
        std::shared_ptr<SessionManager> sess_mgr_;  // SessionManager本身保证线程安全
        std::shared_ptr<RedisMgr> redis_;   // backend的redis管理器
        bool running_{false};
    };
}



#endif 