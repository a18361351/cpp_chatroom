#ifndef HTTP_DBM_DBCONN_HEADER
#define HTTP_DBM_DBCONN_HEADER

#include <string_view>

// #include <boost/mysql.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/tcp.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include "log/log_manager.hpp"
#include "utils/util_class.hpp"
#include "http/dbm/security.hpp"

enum {
    GATEWAY_SUCCESS = 0, 
    GATEWAY_USER_NOT_EXIST = -1, 
    GATEWAY_VERIFY_FAILED = -2, 
    GATEWAY_REG_ALREADY_EXIST = -3,
    GATEWAY_REG_UID_ALREADY_EXIST = -4,
    GATEWAY_UNKNOWN_ERROR = -100,
    GATEWAY_CONNECTION_ERROR = -101,
    GATEWAY_MYSQL_SERVER_ERROR = -102,
};

class DBM;

// TODO(user): boost::mysql中会出现连接中断的情况，我们需要进行这种处理
struct DBConn {
    private:
    void CloseImpl(boost::mysql::error_code& err);

    void ReconnectImpl(boost::mysql::error_code& err, std::string_view username, std::string_view password, std::string_view db_name);

    public:
    DBConn(boost::asio::io_context& ctx, boost::asio::ssl::context& ssl_ctx, std::string_view mysql_addr, uint mysql_port) : conn(ctx, ssl_ctx), mysql_addr_(mysql_addr), mysql_port_(mysql_port) {}
    bool valid_{false};
    boost::mysql::tcp_ssl_connection conn;
    boost::mysql::statement login_check_stmt;
    boost::mysql::statement register_stmt;
    boost::mysql::statement exist_check_stmt;
    std::string_view mysql_addr_;
    uint mysql_port_;

    // 连接与关闭的函数
    void Connect(boost::mysql::error_code& err, std::string_view username, std::string_view password, std::string_view db_name) {
        ReconnectImpl(err, username, password, db_name);
    }
    bool Connect(std::string_view username, std::string_view password, std::string_view db_name) {
        boost::mysql::error_code err;
        ReconnectImpl(err, username, password, db_name);
        if (err) {
            return false; // error when connecting
        }
        return true;
    }

    void Close(boost::mysql::error_code& err) {
        if (valid_) {
            CloseImpl(err);
        }
    }
    bool Close() {
        boost::mysql::error_code err;
        Close(err);
        if (err) {
            return false;
        }
        return true;
    }

    // DBConn业务代码
    // TODO(user): 未来将业务和连接分离
    // @brief 验证用户的用户名和密码是否能与数据库中的对应上
    // @param username 用户名
    // @param passcode 密码，这里传入的密码是明文的，函数内部会自动负责加盐加密并比对
    // @param uid 对应用户的UID，如果验证成功，函数会设置其为登录用户所对应的UID
    // @return 0(GATEWAY_SUCCESS)成功，否则出错
    int VerifyUserInfo(std::string_view username, std::string_view passcode, uint64_t& uid);

    // @brief 尝试着将新用户添加到数据库中
    // @param username 用户名，不可重复
    // @param passcode 密码，这里传入的密码是明文的，存入数据库字段的数据会自动加盐加密
    // @param uid 用户所对应的UID，不可重复
    // @return 0(GATEWAY_SUCCESS)成功，否则出错
    int RegisterNew(std::string_view username, std::string_view passcode, uint64_t uid);
};


#endif