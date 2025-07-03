#include <iostream>

#include "http/req_handler.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;

// 解析请求的部分
boost::beast::http::message_generator request_handler(http::request<boost::beast::http::string_body>&& req) {
    switch(req.method()) {
        case http::verb::get:
            return get_handler(std::move(req));
            // break;
        // case http::verb::head:

        //     break;
        case http::verb::post:
            return post_handler(std::move(req));
            break;
        default:
            return bad_request(std::move(req), "Unsupported HTTP-method for this server");
    }

}

boost::beast::http::message_generator post_handler(http::request<boost::beast::http::string_body>&& req) {
    // GET METHOD
    http::response<http::string_body> resp;
    if (req.target() == "/upload") {
        auto body = req.body();
        std::cout << "Body content = " << body << std::endl;
        resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        resp.set(http::field::content_type, "text/html");
        resp.keep_alive(req.keep_alive());
        resp.body() = body;
        resp.prepare_payload();
        return resp;
    } else {
        return not_found(std::move(req));
    }
}


boost::beast::http::message_generator get_handler(http::request<boost::beast::http::string_body>&& req) {
    // GET METHOD
    http::response<http::string_body> resp;
    if (req.target() == "/test") {
        resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        resp.set(http::field::content_type, "text/html");
        resp.keep_alive(req.keep_alive());
        resp.body() = "Test";
        resp.prepare_payload();
        return resp;
    } else if (req.target() == "/hello") {
        resp.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        resp.set(http::field::content_type, "text/html");
        resp.keep_alive(req.keep_alive());
        resp.body() = "Hello World!";
        resp.prepare_payload();
        return resp;
    } else {
        return not_found(std::move(req));
    }
}