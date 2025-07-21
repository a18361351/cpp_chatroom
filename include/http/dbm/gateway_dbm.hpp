#ifndef HTTP_GATEWAY_DBM_HEADER
#define HTTP_GATEWAY_DBM_HEADER

#include <spdlog/spdlog.h>
#include <string>
#include <queue>

#include <boost/asio.hpp>
#include <boost/mysql.hpp>

#include "utils/util_class.hpp"
#include "http/dbm/security.hpp"
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
    bool Start(uint conns, uint max_conn) {
        if (!running_) {
            if (max_conn < conns) {
                return false;
            }
            for (uint i = 0; i < conns; ++i) {
                conns_.emplace_back(std::make_shared<DBConn>(dbm_ctx_));
                if (!conns_.back()->Connect(username_, password_, db_name_)) {
                    // error when connecting
                    // 当连接池连接出现错误时，我们应该怎么做？不断重新连接？断开其他连接并表示错误？保留现有的连接然后继续下一个？
                    // 当连接出现问题时，很有可能表示网络中断，或者服务器宕机。如果不断重新连接应该是不必要的。
                    // 那么我们直接清理资源，断开所有连接。
                    for (auto& conn : conns_) {
                        conn->Close();
                    }
                    conns_.clear();
                    return false;
                }
            }
            pool_size_ = conns;
            pool_max_cap_ = max_conn;
            running_ = true;
            
            for (auto& conn : conns_) {
                free_queue_.push(conn);
            }

            return true;
        }
        return false;
    }

    // @brief 停止SQL数据库连接池
    // @return true = 成功停止，false = 没有在运行或关闭时出现问题
    bool Stop() {
        if (running_) {
            for (auto& conn : conns_) {
                if (!conn->Close()) {
                    // error when closing
                    // TODO(user): some logging
                    continue;
                }
            }
            conns_.clear();
            running_ = false;
            cv_.notify_all();   // WAKE UP!
            pool_size_ = 0;
            pool_max_cap_ = 0;
            return true;
        }
        return false;
    }

    // Convenient intf
    int VerifyUserInfo(std::string_view username, std::string_view passcode) {
        auto conn = GetIdleConn();
        if (!conn) {
            return GATEWAY_UNKNOWN_ERROR;   // 连接失败，或其他错误
        }
        int ret = conn->VerifyUserInfo(username, passcode);
        ReturnIdleConn(std::move(conn));
        return ret;
    }
    int RegisterNew(std::string_view username, std::string_view passcode) {
        auto conn = GetIdleConn();
        if (!conn) {
            return GATEWAY_UNKNOWN_ERROR;   // 连接失败，或其他错误
        }
        int ret = conn->RegisterNew(username, passcode);
        ReturnIdleConn(std::move(conn));
        return ret;
    }

    // Expose connection obj to user
    ConnWrapper GetIdleConnWrapper() {
        ConnPtr conn = GetIdleConn();
        if (!conn) {
            // no idle connection
            return ConnWrapper(nullptr, this);
        }
        return ConnWrapper(std::move(conn), this);
    }
    
    private:
    // Inner impl
    // @brief 创建新的连接，该函数不检查池子大小，调用者应该自己检查
    ConnPtr CreateConn() {
        ConnPtr conn = make_shared<DBConn>(dbm_ctx_);
        if (!conn->Connect(username_, password_, db_name_)) {
            // error when connecting
            return nullptr;
        }
        conns_.push_back(conn);
        ++pool_size_;
        return conn;
    }

    ConnPtr GetIdleConn() {
        std::unique_lock<std::mutex> lock(latch_);
        while (free_queue_.empty() && running_) {
            if (pool_size_ + 1 <= pool_max_cap_) {
                // 如果连接池未满，则创建新的连接
                ConnPtr new_conn = CreateConn();
                if (new_conn) {
                    free_queue_.push(new_conn);
                } else {
                    // error when creating new connection
                    return nullptr;
                }
            } else {
                // 如果连接池已满，则等待空闲连接
                cv_.wait(lock);
            }
        }
        if (!running_) {
            return nullptr;
        }
        ConnPtr conn = free_queue_.front();
        free_queue_.pop();
        return conn;
    }

    void ReturnIdleConn(ConnPtr&& ptr) {
        if (ptr) {
            std::unique_lock<std::mutex> lock(latch_);
            bool wake = free_queue_.empty();
            free_queue_.push(std::move(ptr));
            if (wake) {
                cv_.notify_one();
            }
        }
    }

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