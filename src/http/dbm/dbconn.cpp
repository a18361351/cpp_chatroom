#include "http/dbm/gateway_dbm.hpp"
#include "http/dbm/dbconn.hpp"

// DBConn object
void DBConn::CloseImpl(boost::mysql::error_code& err) {
    spdlog::debug("CloseImpl called");
    try {
        conn.close_statement(login_check_stmt);
        conn.close_statement(register_stmt);
        conn.close_statement(exist_check_stmt);
    } catch (...) {
        // ignore
        spdlog::error("Failed to close statements in DBConn");
    }
    
    boost::mysql::diagnostics diag;
    conn.close(err, diag);
    valid_ = false;
}

void DBConn::ReconnectImpl(boost::mysql::error_code& err, std::string_view username, std::string_view password, std::string_view db_name) {  // exception
    spdlog::info("ReconnectImpl called with username: {}, db_name: {}", username, db_name);
    // 如果之前已经建立了连接，那么关闭重新连接。作为一次Reconnect行为
    if (valid_) {
        spdlog::debug("ReconnectImpl called on valid connection");
        this->Close();
    }

    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(mysql_addr_), mysql_port_);
    boost::mysql::handshake_params(username, password, db_name);
    // mysql::error_code err;
    boost::mysql::diagnostics diag;
    conn.connect(ep, 
                boost::mysql::handshake_params(username, password, db_name),
                err, diag);
    if (err) {
        // error when connecting
        spdlog::error("Error when connecting: {}", err.message());
        valid_ = false;
        return;
    }

    spdlog::debug("InitStmt");
    // InitStmt
    // 初始化服务器端的语句
    // FIXME(user): 混用了异常和err，很难看，但是懒得修了
    try {
        login_check_stmt = conn.prepare_statement("SELECT (passcode) FROM tbl_user WHERE username = ?");
        exist_check_stmt = conn.prepare_statement("SELECT COUNT(*) FROM tbl_user WHERE username = ?");
        register_stmt = conn.prepare_statement("INSERT INTO tbl_user (username, passcode) VALUES (?, ?)");
    } catch (const boost::mysql::error_code& err) {
        // 关闭连接防止连接泄漏
        spdlog::error("Failed to prepare statements in DBConn: {}", err.message());
        this->Close();
        valid_ = false;
    }

    // 连接成功
    valid_ = true;

}

// DBConn业务代码
// TODO(user): 未来将业务和连接分离
// @brief 验证用户的用户名和密码是否能与数据库中的对应上
// @param username 用户名
// @param passcode 密码，这里传入的密码是明文的，函数内部会自动负责加盐加密并比对
// @return 0(GATEWAY_SUCCESS)成功，否则出错
int DBConn::VerifyUserInfo(std::string_view username, std::string_view passcode) {
    // we use string_view in C++17!
    boost::mysql::results ret;
    // TODO(user): 完善报错逻辑
    boost::mysql::error_code err;
    [[maybe_unused]]boost::mysql::diagnostics diag;
    conn.execute_statement(login_check_stmt, std::tuple(username), ret, err, diag);
    if (err) {
        // error when executing SQL
        spdlog::error("Mysql error in verify(login_check_stmt): {} {}", err.what(), diag.server_message());
        return GATEWAY_MYSQL_SERVER_ERROR; // internal error
    }
    if (ret.rows().empty()) {
        // TODO(user): 这里是否需要进行一个故意的空Verify过程，以进一步降低恶意客户端暴力遍历获取有效用户名的可能性？
        // user not found
        return GATEWAY_USER_NOT_EXIST; // user not found
    }
    auto correct_code_hash = ret.rows().at(0).at(0).get_string();
    // is user provided passcode matched with passcode in db?
    bool match = Security::Verify(passcode, correct_code_hash);
    return match ? GATEWAY_SUCCESS : GATEWAY_VERIFY_FAILED; // 0 = success, -2 = verify failed
}

// @brief 尝试着将新用户添加到数据库中
// @param username 用户名
// @param passcode 密码，这里传入的密码是明文的，存入数据库字段的数据会自动加盐加密
// @return 0(GATEWAY_SUCCESS)成功，否则出错
int DBConn::RegisterNew(std::string_view username, std::string_view passcode) {
    // check if name exists
    boost::mysql::results ret;
    boost::mysql::error_code err;
    [[maybe_unused]]boost::mysql::diagnostics diag;
    conn.execute_statement(exist_check_stmt, std::tuple(username), ret, err, diag);
    if (err) {
        // error when executing SQL
        spdlog::error("Mysql error in register(exist_check_stmt): {} {}", err.what(), diag.server_message());
        return GATEWAY_MYSQL_SERVER_ERROR; // internal error
    }
    if (ret.rows().at(0).at(0).get_int64() > 0) {
        return GATEWAY_REG_ALREADY_EXIST; // user already exists
    }

    // create new user
    std::string code_hash = Security::HashPassword(passcode);
    conn.execute_statement(register_stmt, std::tuple(username, code_hash), ret, err, diag);
    if (err) {
        // error when executing SQL
        spdlog::error("Mysql error in register(register_stmt): {} {}", err.what(), diag.server_message());
        return GATEWAY_MYSQL_SERVER_ERROR; // internal error
    }
    if (ret.has_value()) {
        return GATEWAY_SUCCESS; // 0 = success
    }
    // unknown error
    return GATEWAY_UNKNOWN_ERROR; // unknown error
}

// ConnWrapper
ConnWrapper::~ConnWrapper() {
    if (dbm_ && conn_) {
        // 如果dbm本身不再运行了，那么直接关闭连接即可（dbm已经关闭了），不必再归还连接
        if (dbm_->running_) {
            dbm_->ReturnIdleConn(std::move(conn_));
        }
    }
}