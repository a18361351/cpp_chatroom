#include <iostream>

#include "http/req_handler.hpp"

#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/json.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;

// FIXME(user): 返回的body最好是json

// 解析请求的部分
boost::beast::http::message_generator ReqHandler::request_handler(http::request<boost::beast::http::string_body>&& req) {
    switch(req.method()) {
        case http::verb::get:
            return get_handler(std::move(req));
        case http::verb::post:
            return post_handler(std::move(req));
        default:
            return bad_request(std::move(req), "Unsupported HTTP-method for this server");
    }

}

boost::beast::http::message_generator ReqHandler::post_handler(http::request<boost::beast::http::string_body>&& req) {
    // GET METHOD
    http::response<http::string_body> resp;
    if (req.target() == "/login") {
        // 登录验证逻辑
        Json::Reader rr;
        Json::Value readed;

        bool ret = rr.parse(req.body(), readed, false);
        if (!ret) {
            return bad_request(std::move(req), "Invalid request format");
        }
        string username, passcode;
        try {
            username = readed["username"].asString();
            passcode = readed["passcode"].asString();
        } catch (...) {
            return bad_request(std::move(req), "Invalid request format");
        }
        int db_ret = dbm_->VerifyUserInfo(username, passcode);
        switch (db_ret) {
            case GATEWAY_SUCCESS:
                // TODO(user): 未来进行负载均衡，告诉用户其token以及服务器ip地址，但是这里我们先进行登录的测试吧……一步一步来
                // TODO(user): 限流器
                // TODO(user): 返回json格式
                resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                resp.set(http::field::content_type, "text/html");
                resp.result(http::status::ok);
                resp.keep_alive(req.keep_alive());
                resp.body() = "Login success!";
                resp.prepare_payload();
                return resp;
            case GATEWAY_USER_NOT_EXIST: case GATEWAY_VERIFY_FAILED:
                return forbidden_request(std::move(req), "Incorrect login username or password");
            case GATEWAY_MYSQL_SERVER_ERROR:
                return bad_request(std::move(req), "MySQL server error");
            case GATEWAY_UNKNOWN_ERROR: default:
                return bad_request(std::move(req), "Server error");
        }
    } else if (req.target() == "/register") {
        // 注册逻辑
        // TODO(user): 未来加入验证码功能，以及限流功能，防止恶意注册
        Json::Reader rr;
        Json::Value readed;

        bool ret = rr.parse(req.body(), readed, false);
        if (!ret) {
            return bad_request(std::move(req), "Invalid request format");
        }
        string username, passcode;
        try {
            username = readed["username"].asString();
            passcode = readed["passcode"].asString();
        } catch (...) {
            return bad_request(std::move(req), "Invalid request format");
        }
        int db_ret = dbm_->RegisterNew(username, passcode);
        switch (db_ret) {
            case GATEWAY_SUCCESS:
                resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                resp.set(http::field::content_type, "text/html");
                resp.result(http::status::ok);
                resp.keep_alive(req.keep_alive());
                resp.body() = "New user " + username + " registered successfully!";
                resp.prepare_payload();
                return resp;
            case GATEWAY_REG_ALREADY_EXIST:
                return forbidden_request(std::move(req), "Username already exists");   
            case GATEWAY_MYSQL_SERVER_ERROR:
                return bad_request(std::move(req), "MySQL server error");
            case GATEWAY_UNKNOWN_ERROR: default:
                return bad_request(std::move(req), "Server error");
        }
    } else if (req.target() == "/ping") {
        resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        resp.set(http::field::content_type, "text/html");
        resp.result(http::status::ok);
        resp.keep_alive(req.keep_alive());
        resp.body() = "pong";
        resp.prepare_payload();
        return resp;
    } else {
        return not_found(std::move(req));
    }
}


boost::beast::http::message_generator ReqHandler::get_handler(http::request<boost::beast::http::string_body>&& req) {
    // GET METHOD
    http::response<http::string_body> resp;
    if (req.target() == "/login") {
        return bad_request(std::move(req), "Bad method for login");
    } else if (req.target() == "/register") {
        return bad_request(std::move(req), "Bad method for register");
    } else if (req.target() == "/ping") {
        resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        resp.set(http::field::content_type, "text/html");
        resp.result(http::status::ok);
        resp.keep_alive(req.keep_alive());
        resp.body() = "pong";
        resp.prepare_payload();
        return resp;
    } else {
        return not_found(std::move(req));
    }
}