#include <grpcpp/support/status.h>
#include <iostream>

#include "http/req_handler.hpp"

#include <json/value.h>
#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/json.h>
#include <sys/types.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;

// FIXME(user): 返回的body最好是json

// 解析请求的部分
boost::beast::http::message_generator chatroom::gateway::ReqHandler::request_handler(http::request<boost::beast::http::string_body>&& req) {
    switch(req.method()) {
        case http::verb::get:
            return get_handler(std::move(req));
        case http::verb::post:
            return post_handler(std::move(req));
        default:
            return bad_request(std::move(req), "Unsupported HTTP-method for this server");
    }

}

inline string TokenGenerator() {
    // 随机字节序列
    vector<unsigned char> token_bytes(24);
    assert(RAND_bytes(token_bytes.data(), token_bytes.size()) == 1);
    // base64
    size_t len = 4 * ((token_bytes.size() + 2) / 3);
    string token(len, 0);

    EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&token[0]), token_bytes.data(), token_bytes.size());   // NOLINT
    // 转换为URL安全格式
    for (char& c : token) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    token.erase(remove(token.begin(), token.end(), '='), token.end());
    return token;
}

// 对/login的POST请求
boost::beast::http::message_generator chatroom::gateway::ReqHandler::LoginLogic(http::request<boost::beast::http::string_body>&& req) {
    // TODO(user): 为重复登录的情况作检查

    http::response<http::string_body> resp;
    // 登录验证逻辑
    Json::Reader rr;
    Json::Value readed;

    // 从客户端中发送的JSON请求中读取信息
    bool ret = rr.parse(req.body(), readed, false);
    if (!ret) {
        spdlog::error("Error occured when parsing json");
        return bad_request(std::move(req), "Invalid request format");
    }
    string username, passcode;
    try {
        username = readed["username"].asString();
        passcode = readed["passcode"].asString();
    } catch (...) {
        spdlog::error("JSON format doesn't match");
        return bad_request(std::move(req), "Invalid request format");
    }

    // 传入MySQL数据库进行身份验证
    spdlog::info("User {} attempt to login", username);
    uint64_t uid = 0;
    int db_ret = dbm_->VerifyUserInfo(username, passcode, uid);
    switch (db_ret) {
        case GATEWAY_SUCCESS:
            // 登录成功
            spdlog::info("User {} verified successfully", username);        // 身份验证成功，下一步
            break;
        case GATEWAY_USER_NOT_EXIST: case GATEWAY_VERIFY_FAILED:
            spdlog::info("Incorrect login attempt by user {}", username);
            return forbidden_request(std::move(req), "Incorrect login username or password");   // 对客户隐藏具体的错误信息
        case GATEWAY_MYSQL_SERVER_ERROR:
            spdlog::error("Mysql server error when user {} login", username);
            return server_error(std::move(req), "Server error");
        case GATEWAY_UNKNOWN_ERROR: default:
            spdlog::error("Unknown error when user {} login", username);
            return server_error(std::move(req), "Server error");
    }

    // 验证成功，通过RPC获取服务器信息
    auto stub = rpc_->GetStatusStub();
    grpc::ClientContext ctx;
    chatroom::status::MinimalLoadServerReq rpc_req;
    chatroom::status::ServerAddrResp rpc_resp;
    grpc::Status rpc_status = stub->CheckMinimalLoadServer(&ctx, rpc_req, &rpc_resp);
    if (!rpc_status.ok()) {
        spdlog::error("Status RPC call failed: {}", rpc_status.error_message());
        return server_error(std::move(req), "Server error"); // 对客户隐藏具体的错误信息
    }
    auto addr = rpc_resp.server_addr();

    // 更新用户在Redis缓存中的信息
    if (!redis_->UpdateUserInfo(std::to_string(uid), username)) {
        spdlog::warn("Redis UpdateUserInfo failed");
    }

    // 检查用户的登录状态
    auto check_result = redis_->UserLoginAttempt(std::to_string(uid));
    if (!check_result.first) { // 尝试登录请求失败：用户已登录或无法查询在线状态
        if (!check_result.second.has_value()) { // 无法查询在线状态
            spdlog::error("Redis UserLoginAttempt failed");
            return server_error(std::move(req), "Server error");
        }
        // 否则，就是用户已登录的情况
        // TODO(user): 实现强制下线功能
        spdlog::info("User already logined, force kick currently online user {}", username);
        return forbidden_request(std::move(req), "User already login, we will implement force-login logic later~");
    }

    // 生成用户的token
    string token = TokenGenerator();
    redis_->RegisterUserToken(token, std::to_string(uid), 50); // 默认50秒的token存活时间
    
    // 现在，把token以及服务器地址打包，发送给用户
    resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    resp.set(http::field::content_type, "application/json");
    resp.result(http::status::ok);
    resp.keep_alive(req.keep_alive());
    Json::Value resp_json;
    Json::StreamWriterBuilder writer;
    resp_json["token"] = token;
    resp_json["server_addr"] = addr;
    resp_json["uid"] = uid;
    resp.body() = Json::writeString(writer, resp_json);
    resp.prepare_payload();
    return resp;
}

boost::beast::http::message_generator chatroom::gateway::ReqHandler::PostRegisterLogic(http::request<boost::beast::http::string_body>&& req) {
    http::response<http::string_body> resp;
    // 注册逻辑
    // TODO(user): 未来加入验证码功能，以及限流功能，防止恶意注册
    Json::Reader rr;
    Json::Value readed;

    bool ret = rr.parse(req.body(), readed, false);
    if (!ret) {
        spdlog::error("Error occured when parsing json");
        return bad_request(std::move(req), "Invalid request format");
    }
    string username, passcode;
    try {
        username = readed["username"].asString();
        passcode = readed["passcode"].asString();
    } catch (...) {
        spdlog::error("JSON format doesn't match");
        return bad_request(std::move(req), "Invalid request format");
    }
    uint64_t uid = uid_gen_.Generate();
    spdlog::info("Attempt to register a new user {}", username);
    int db_ret = dbm_->RegisterNew(username, passcode, uid);
    switch (db_ret) {
        case GATEWAY_SUCCESS:
            spdlog::info("User {} registered successfully, uid = {}", username, uid);
            break;
        case GATEWAY_REG_ALREADY_EXIST:
            spdlog::info("Duplicated register attempt by username {}", username);
            return forbidden_request(std::move(req), "Username already exists");   
        case GATEWAY_MYSQL_SERVER_ERROR:
            spdlog::error("Mysql server error when user {} register", username);
            return server_error(std::move(req), "MySQL server error");
        case GATEWAY_UNKNOWN_ERROR: default:
            spdlog::error("Unknown error when user {} register", username);
            return server_error(std::move(req), "Server error");
    }
    Json::Value resp_json;
    Json::StreamWriterBuilder writer;
    resp_json["result"] = 0;
    resp_json["message"] = "success";

    resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    resp.set(http::field::content_type, "application/json");
    resp.result(http::status::ok);
    resp.keep_alive(req.keep_alive());
    resp.body() = Json::writeString(writer, resp_json);
    resp.prepare_payload();
    return resp;
}

boost::beast::http::message_generator chatroom::gateway::ReqHandler::PingLogic(http::request<boost::beast::http::string_body>&& req) {
    http::response<http::string_body> resp;
    resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    resp.set(http::field::content_type, "text/html");
    resp.result(http::status::ok);
    resp.keep_alive(req.keep_alive());
    resp.body() = "pong";
    resp.prepare_payload();
    return resp;
}

boost::beast::http::message_generator chatroom::gateway::ReqHandler::post_handler(http::request<boost::beast::http::string_body>&& req) {
    // GET METHOD
    if (req.target() == "/login") {
        return LoginLogic(std::move(req));
    } else if (req.target() == "/register") {
        return PostRegisterLogic(std::move(req));
    } else if (req.target() == "/ping") {
        return PingLogic(std::move(req));
    } else {
        return not_found(std::move(req));
    }
}

boost::beast::http::message_generator chatroom::gateway::ReqHandler::get_handler(http::request<boost::beast::http::string_body>&& req) {
    // GET METHOD
    http::response<http::string_body> resp;
    if (req.target() == "/login") {
        return bad_request(std::move(req), "Bad method for login");
    } else if (req.target() == "/register") {
        return bad_request(std::move(req), "Bad method for register");
    } else if (req.target() == "/ping") {
        return PingLogic(std::move(req));
    } else {
        return not_found(std::move(req));
    }
}