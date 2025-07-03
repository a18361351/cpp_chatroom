#ifndef HTTP_REQUEST_HANDLER_HEADER
#define HTTP_REQUEST_HANDLER_HEADER

#include <boost/beast.hpp>


// boost::beast::http::message_generator：用于延迟生成HTTP消息字节流的工具，可由http::response转换而成

// 对请求头部进行解析，并根据METHOD交给对应的handler处理
boost::beast::http::message_generator request_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);

// 对应方法的handler
boost::beast::http::message_generator get_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);
boost::beast::http::message_generator post_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);

// Returns a bad request response (400)
template <typename Body>
boost::beast::http::message_generator bad_request(boost::beast::http::request<Body>&& req, std::string_view prompt) {
    boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::bad_request, req.version()};
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(prompt);
    res.prepare_payload();
    return res;
}

// Returns a not found response (404)
template <typename Body>
boost::beast::http::message_generator not_found(boost::beast::http::request<Body>&& req) {
    boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::not_found, req.version()};
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(req.target()) + "' was not found.";
    res.prepare_payload();
    return res;
}

// Returns a server error response (500)
template <typename Body>
boost::beast::http::message_generator server_error(boost::beast::http::request<Body>&& req, std::string_view what) {
    boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::internal_server_error, req.version()};
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "An error occurred: '" + std::string(what) + "'";
    res.prepare_payload();
    return res;
};


#endif