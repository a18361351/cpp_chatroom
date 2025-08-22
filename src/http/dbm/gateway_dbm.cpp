
#include "http/dbm/gateway_dbm.hpp"

#include <boost/mysql.hpp>

#include "http/dbm/security.hpp"

using ConnPtr = std::shared_ptr<DBConn>;

bool DBM::Start(uint conns, uint max_conn) {
    if (!running_) {
        spdlog::info("DBM started, conns = {}", conns);
        if (max_conn < conns) {
            return false;
        }
        for (uint i = 0; i < conns; ++i) {
            conns_.emplace_back(std::make_shared<DBConn>(dbm_ctx_, ssl_ctx_, mysql_addr_, mysql_port_));
            if (!conns_.back()->Connect(username_, password_, db_name_)) {
                // error when connecting
                // 当连接池连接出现错误时，我们应该怎么做？不断重新连接？断开其他连接并表示错误？保留现有的连接然后继续下一个？
                // 当连接出现问题时，很有可能表示网络中断，或者服务器宕机。如果不断重新连接应该是不必要的。
                // 那么我们直接清理资源，断开所有连接。
                for (auto &conn : conns_) {
                    conn->Close();
                }
                conns_.clear();
                return false;
            }
        }
        pool_size_ = conns;
        pool_max_cap_ = max_conn;
        running_ = true;

        for (auto &conn : conns_) {
            free_queue_.push(conn);
        }

        return true;
    }
    return false;
}

bool DBM::Stop() {
    if (running_) {
        for (auto &conn : conns_) {
            if (!conn->Close()) {
                // error when closing
                // TODO(user): some logging
                continue;
            }
        }
        conns_.clear();
        running_ = false;
        cv_.notify_all();  // WAKE UP!
        pool_size_ = 0;
        pool_max_cap_ = 0;
        return true;
    }
    return false;
}

int DBM::VerifyUserInfo(std::string_view username, std::string_view passcode, uint64_t &uid) {
    auto conn = GetIdleConn();
    if (!conn) {
        return GATEWAY_UNKNOWN_ERROR;  // 连接失败，或其他错误
    }
    int ret = conn->VerifyUserInfo(username, passcode, uid);
    ReturnIdleConn(std::move(conn));
    return ret;
}

int DBM::RegisterNew(std::string_view username, std::string_view passcode, uint64_t uid) {
    auto conn = GetIdleConn();
    if (!conn) {
        return GATEWAY_UNKNOWN_ERROR;  // 连接失败，或其他错误
    }
    int ret = conn->RegisterNew(username, passcode, uid);
    ReturnIdleConn(std::move(conn));
    return ret;
}

ConnPtr DBM::CreateConn() {
    ConnPtr conn = make_shared<DBConn>(dbm_ctx_, ssl_ctx_, mysql_addr_, mysql_port_);
    if (!conn->Connect(username_, password_, db_name_)) {
        // error when connecting
        return nullptr;
    }
    conns_.push_back(conn);
    ++pool_size_;
    return conn;
}

ConnPtr DBM::GetIdleConn() {
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

void DBM::ReturnIdleConn(ConnPtr &&ptr) {
    if (ptr) {
        std::unique_lock<std::mutex> lock(latch_);
        bool wake = free_queue_.empty();
        free_queue_.push(std::move(ptr));
        if (wake) {
            cv_.notify_one();
        }
    }
}