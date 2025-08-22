#ifndef BACKEND_MSGQUEUE_HANDLER_HEADER
#define BACKEND_MSGQUEUE_HANDLER_HEADER

#include "common/msgnode.hpp"
#include "server/redis/server_redis.hpp"
#include "server/session_manager.hpp"
#include "utils/field_op.hpp"

namespace chatroom::backend {
    class MQHandler {
        public:
        MQHandler(uint32_t server_id, std::shared_ptr<SessionManager> sess, std::shared_ptr<RedisMgr> redis) :
            server_id_(std::to_string(server_id))
            , sess_(std::move(sess))
            , redis_(std::move(redis)) 
        {
            running_ = true;
            redis_->RegisterMsgQueue(server_id_, true);
            worker_ = std::thread([this] {
                this->WorkerFn();
            });
        }
        ~MQHandler() {
            running_ = false;
            if (worker_.joinable()) {
                worker_.join();
            }
        }

        private:
        void WorkerFn();
        void MessageHandler(RedisMgr::Item& item);
        void CtrlMsgHandler(RedisMgr::Item& item);
        std::atomic_bool running_;
        std::thread worker_;
        std::string server_id_;
        std::shared_ptr<SessionManager> sess_;
        std::shared_ptr<RedisMgr> redis_;
    };

}

#endif