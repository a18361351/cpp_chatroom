#ifndef HTTP_GATEWAY_DBM_HEADER
#define HTTP_GATEWAY_DBM_HEADER

#include <string>
#include <string_view>
#include <queue>
#include <condition_variable>
#include <vector>

// #include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>

#include "utils/util_class.hpp"
#include "http/dbm/dbconn.hpp"
#include "log/log_manager.hpp"


// TODO(user): LOG!!!!!!!


// 管理数据库连接的类，隐藏了数据库连接的细节
class DBM : public Noncopyable {
    public:
    friend class ConnWrapper;
    DBM(const std::string& username, 
        const std::string& password, const std::string& db_name,
        const std::string& mysql_addr, uint mysql_port)
        : dbm_ctx_(), pool_size_(0), pool_max_cap_(0),
          username_(username), password_(password), db_name_(db_name),
          mysql_addr_(mysql_addr), mysql_port_(mysql_port) {
            spdlog::debug("DBM created");
          }
    ~DBM() {
        spdlog::debug("DBM destroyed");
    }

    public:
    using ConnPtr = std::shared_ptr<DBConn>;  


    public:
    // class DBM
    
    // Start and stop

    // @brief 启动SQL数据库连接池
    // @param conns 连接池的连接数
    // @return true = 成功启动，false = 已经在运行或连接失败
    bool Start(uint conns, uint max_conn);

    // @brief 停止SQL数据库连接池
    // @return true = 成功停止，false = 没有在运行或关闭时出现问题
    bool Stop();

    // Convenient intf
    int VerifyUserInfo(std::string_view username, std::string_view passcode);
    int RegisterNew(std::string_view username, std::string_view passcode);

    // Expose connection obj to user
    ConnWrapper GetIdleConnWrapper();
    
    private:
    // Inner impl
    // @brief 创建新的连接，该函数不检查池子大小，调用者应该自己检查
    ConnPtr CreateConn();
    ConnPtr GetIdleConn();
    void ReturnIdleConn(ConnPtr&& ptr);

    // 连接对象可能共享给其他人，同时有可能出现Stop()之后，仍有正在运行的SQL操作的情况。我们必须通过某种方式控制连接的生命周期（这里先选择shared_ptr）
    boost::asio::io_context dbm_ctx_;
    std::vector<ConnPtr> conns_;
    std::queue<ConnPtr> free_queue_;    // 无操作的连接队列
    std::mutex latch_;  // 保护并发安全的互斥锁
    std::condition_variable cv_;    // 用于唤醒消费者的条件变量（消费者是阻塞等待空闲连接的线程）
    uint pool_size_;        // 当前池子的连接数量
    uint pool_max_cap_;     // 池子的最大连接数量
    // DBM中保存的服务器数据
    std::string username_;
    std::string password_;
    std::string db_name_;
    std::string mysql_addr_;
    uint mysql_port_;

    bool running_{false};
};

#endif