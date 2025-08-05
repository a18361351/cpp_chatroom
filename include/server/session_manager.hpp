#ifndef BACKEND_SESSION_MANAGER_HEADER
#define BACKEND_SESSION_MANAGER_HEADER

#include <memory>

#include "server/session.hpp"

class Session;

namespace chatroom::backend {
    class SessionManager {
        public:

        // @brief 停止Session的运行，并将其移除出去
        bool StopSession(UID sess_id);

        // @brief 添加Session对象到管理器中
        bool AddSession(UID sess_id, std::shared_ptr<Session> sess);
        
        // @brief 获取对应的Session对象
        std::shared_ptr<Session> GetSession(UID sess_id);

        // @brief 获取当前SessionManager中所有有效的Session的个数
        uint32_t GetSessionCount() const;

        private:
        std::unordered_map<UID, std::shared_ptr<Session>> sess_;    // session_id -> Session对象
        std::mutex lck_;    // 并发访问控制
    };
}

#endif