#include "http/dbm/dbconn.hpp"

#include "http/dbm/gateway_dbm.hpp"

// DBConn object
void DBConn::CloseImpl(boost::mysql::error_code &err) {
    spdlog::debug("CloseImpl called");
    try {
        conn_.close_statement(login_check_stmt_);
        conn_.close_statement(register_stmt_);
        conn_.close_statement(exist_check_stmt_);
    } catch (...) {
        // ignore
        spdlog::error("Failed to close statements in DBConn");
    }

    boost::mysql::diagnostics diag;
    conn_.close(err, diag);
    valid_ = false;
}

void DBConn::ReconnectImpl(boost::mysql::error_code &err, std::string_view username, std::string_view password,
                           std::string_view db_name) {  // exception
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
    conn_.connect(ep, boost::mysql::handshake_params(username, password, db_name), err, diag);
    if (err) {
        // error when connecting
        spdlog::error("Error when connecting: {}", err.message());
        valid_ = false;
        return;
    }

    spdlog::debug("InitStmt");
    // InitStmt
    // 初始化服务器端的语句
    try {
        login_check_stmt_ = conn_.prepare_statement("SELECT uid, passcode FROM tbl_user WHERE username = ?");
        exist_check_stmt_ = conn_.prepare_statement("SELECT COUNT(*) FROM tbl_user WHERE username = ?");
        register_stmt_ = conn_.prepare_statement("INSERT INTO tbl_user (uid, username, passcode) VALUES (?, ?, ?)");
    } catch (const boost::mysql::error_with_diagnostics &ec) {
        // 关闭连接防止连接泄漏
        spdlog::error("Failed to prepare statements in DBConn: {}, db diag: {}", ec.what(),
                      ec.get_diagnostics().server_message());
        this->Close();
        // valid_ = false;
    }

    // 连接成功
    valid_ = true;
}

int DBConn::VerifyUserInfo(std::string_view username, std::string_view passcode, uint64_t &uid) {
    // we use string_view in C++17!
    boost::mysql::results ret;
    // TODO(user): 完善报错逻辑
    boost::mysql::error_code err;
    [[maybe_unused]] boost::mysql::diagnostics diag;
    conn_.execute_statement(login_check_stmt_, std::tuple(username), ret, err, diag);
    if (err) {
        // error when executing SQL
        spdlog::error("Mysql error in verify(login_check_stmt_): {} {}", err.what(), diag.server_message());
        return GATEWAY_MYSQL_SERVER_ERROR;  // internal error
    }
    if (ret.rows().empty()) {
        // user not found
        return GATEWAY_USER_NOT_EXIST;  // user not found
    }
    auto correct_code_hash = ret.rows().at(0).at(1).get_string();
    auto get_uid = ret.rows().at(0).at(0).get_uint64();
    // is user provided passcode matched with passcode in db?
    bool match = Security::Verify(passcode, correct_code_hash);
    if (match) {
        uid = get_uid;
        return GATEWAY_SUCCESS;
    }
    return GATEWAY_VERIFY_FAILED;  // 0 = success, -2 = verify failed
}

// @brief 尝试着将新用户添加到数据库中
// @param username 用户名
// @param passcode 密码，这里传入的密码是明文的，存入数据库字段的数据会自动加盐加密
// @return 0(GATEWAY_SUCCESS)成功，否则出错
int DBConn::RegisterNew(std::string_view username, std::string_view passcode, uint64_t uid) {
    // check if name exists
    boost::mysql::results ret;
    boost::mysql::error_code err;
    [[maybe_unused]] boost::mysql::diagnostics diag;
    conn_.execute_statement(exist_check_stmt_, std::tuple(username), ret, err, diag);
    if (err) {
        // error when executing SQL
        spdlog::error("Mysql error in register(exist_check_stmt_): {} {}", err.what(), diag.server_message());
        return GATEWAY_MYSQL_SERVER_ERROR;  // internal error
    }
    if (ret.rows().at(0).at(0).get_int64() > 0) {
        return GATEWAY_REG_ALREADY_EXIST;  // user already exists
    }

    // create new user
    std::string code_hash = Security::HashPassword(passcode);
    conn_.execute_statement(register_stmt_, std::tuple(uid, username, code_hash), ret, err, diag);
    if (err) {
        // error when executing SQL
        spdlog::error("Mysql error in register(register_stmt_): {} {}", err.what(), diag.server_message());
        return GATEWAY_MYSQL_SERVER_ERROR;  // internal error
    }
    if (ret.has_value()) {
        return GATEWAY_SUCCESS;  // 0 = success
    }
    // unknown error
    return GATEWAY_UNKNOWN_ERROR;  // unknown error
}
