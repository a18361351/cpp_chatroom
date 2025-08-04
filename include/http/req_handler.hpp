#ifndef HTTP_REQUEST_HANDLER_HEADER
#define HTTP_REQUEST_HANDLER_HEADER

#include <memory>

#include <boost/asio/thread_pool.hpp>
// #include <boost/beast.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/string_body.hpp>

#include "http/rpc/status_rpc_client.hpp"
#include "log/log_manager.hpp"
#include "http/dbm/gateway_dbm.hpp"
#include "http/redis/gateway_redis.hpp"

namespace chatroom::gateway {
    // req_handler: 内部维护一个队列/线程池，将接收到的请求分发给下层 
    // 对客户端发送来的请求进行解析处理，并分发给下一层（逻辑处理层）
    // boost::beast::http::message_generator：用于延迟生成HTTP消息字节流的工具，可由http::response转换而成
    
    class ReqHandler : public std::enable_shared_from_this<ReqHandler> {
        public:
        explicit ReqHandler(std::shared_ptr<DBM> dbm, std::shared_ptr<RedisMgr> redis, std::shared_ptr<StatusRPCClient> rpc, uint pool_size = 4) : 
            dbm_(std::move(dbm)),
            redis_(std::move(redis)),
            rpc_(std::move(rpc)),
            pool_(pool_size) {}
    
        ~ReqHandler() {
            pool_.stop();
            spdlog::info("Thread pool joining...");
            pool_.join();
        }
    
        // callback func: bool func(boost::beast::http::message_generator resp, bool error(unused));
        using RespCallback = std::function<bool(boost::beast::http::message_generator, bool)>;
        
        // 通过异步的线程池，解耦分离请求发送接收和请求处理部分
        void PostRequest(boost::beast::http::request<boost::beast::http::string_body>&& req, RespCallback cb) {
            boost::asio::post(pool_, [self = shared_from_this(), req = std::move(req), cb = std::move(cb)]() mutable {
                [[maybe_unused]]bool ret = cb(self->request_handler(std::move(req)), false);
            });
        }
        
        private:
        // 将dbm和redis类注入ReqHandler
        // mysql connection manager
        std::shared_ptr<DBM> dbm_;
        std::shared_ptr<RedisMgr> redis_;
        std::shared_ptr<StatusRPCClient> rpc_;
        
        // 异步接受请求对象，需要使用线程池
        boost::asio::thread_pool pool_;
        
        // 对请求头部进行解析，并根据METHOD交给对应的handler处理
        boost::beast::http::message_generator request_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);
        // 对应方法的handler
        boost::beast::http::message_generator get_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);
        boost::beast::http::message_generator post_handler(boost::beast::http::request<boost::beast::http::string_body>&& req);
    
        // 请求处理的逻辑体部分
        boost::beast::http::message_generator LoginLogic(boost::beast::http::request<boost::beast::http::string_body>&& req);
        boost::beast::http::message_generator PostRegisterLogic(boost::beast::http::request<boost::beast::http::string_body>&& req);
        boost::beast::http::message_generator PingLogic(boost::beast::http::request<boost::beast::http::string_body>&& req);
    };
    
    
    
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
    
    // Returns a forbidden response (403)
    template <typename Body>
    boost::beast::http::message_generator forbidden_request(boost::beast::http::request<Body>&& req, std::string_view prompt) {
        boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::forbidden, req.version()};
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
        res.body() = std::string(what);
        res.prepare_payload();
        return res;
    };
}


#endif