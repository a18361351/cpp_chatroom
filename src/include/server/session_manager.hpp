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

        // @brief 验证后的会话可以添加到会话管理器中
        bool AddSession(UID sess_id, std::shared_ptr<Session> sess);

        // @brief 添加Session对象到会话临时存储中
        bool AddTempSession(std::shared_ptr<Session> sess);

        // @brief 当临时会话的验证过程失败时，调用此函数将临时会话移除
        // @ 注：不用担心裸指针问题，该裸指针仅仅作为哈希表的key使用，函数不访问指针指向地址
        // @warning 因此，调用者需要手动关闭会话连接
        bool RemoveTempSession(Session* sess_ptr);


        // @brief 移除会话对象
        // @warning 该方法不会关闭会话本身，只是将其从列表移除；如果需要关闭会话，请调用Session::Close()
        bool RemoveSession(UID sess_id);
        
        // @brief 获取对应的Session对象
        std::shared_ptr<Session> GetSession(UID sess_id);

        // @brief 获取当前管理器中所有有效的Session的个数
        uint32_t GetSessionCount();

        // @brief 获取当前管理器中所有临时的Session的个数
        uint32_t GetTempSessionCount();

        // @brief 获取当前管理器中总共的Session的个数
        uint32_t GetTotalSessionCount();

        private:
        std::unordered_map<UID, std::shared_ptr<Session>> sess_;    // session_id -> Session对象
        std::unordered_map<Session*, std::shared_ptr<Session>> temp_sess_;  // 临时存放的Session对象序列（需要身份验证）
        std::mutex lck_;    // 并发访问控制
        std::mutex temp_lck_;
    };
}

#endif