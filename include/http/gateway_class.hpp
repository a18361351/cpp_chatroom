#ifndef HTTP_GATEWAY_CLASS_HEADER
#define HTTP_GATEWAY_CLASS_HEADER

// gateway_class: 负责实际进行网关的职责：登陆验证+负载均衡
//  登陆验证中，我们使用了OpenSSL加密套件中的PBKDF2加密

#include <boost/asio/io_context.hpp>
#include <sw/redis++/connection.h>
#include <sw/redis++/connection_pool.h>

#include "http/req_handler.hpp"
#include "utils/util_class.hpp"
#include "http/dbm/gateway_dbm.hpp"
#include "http/redis/gateway_redis.hpp"
#include "http/http_server.hpp"
#include "http/gateway_rpc.hpp"

const std::string mysql_addr = "192.168.56.101";
const int mysql_port = 3306;

const std::string username = "lance";
const std::string password = "123456";
const std::string db_name = "chat";

namespace chatroom::gateway {
    // 网关服务器的一个实例
    class GatewayClass : public Noncopyable /* 或者GatewayApp */ {
        public:
        void Run(uint db_conn);
        explicit GatewayClass(boost::asio::io_context& http_ctx) :
            http_ctx_(http_ctx) 
        {
            spdlog::info("GatewayClass created");
        }

        void Initialize(const boost::asio::ip::tcp::endpoint& http_ep, 
                     const sw::redis::ConnectionOptions& redis_conn_opt,
                     const sw::redis::ConnectionPoolOptions& redis_pool_opt,
                     const std::string& status_ep) 
        {
            dbm_ = std::make_shared<DBM>(username, password, db_name, mysql_addr, mysql_port);
            redis_mgr_ = std::make_shared<RedisMgr>();
            redis_mgr_->ConnectTo(redis_conn_opt, redis_pool_opt);
            status_rpc_ = std::make_shared<StatusRPCClient>(status_ep);
            handler_ = std::make_shared<ReqHandler>(dbm_, redis_mgr_, status_rpc_);
            http_ = std::make_shared<HTTPServer>(http_ctx_, http_ep, handler_);
        }

        GatewayClass(boost::asio::io_context& http_ctx, 
                     const boost::asio::ip::tcp::endpoint& http_ep, 
                     const std::string& redis_ep,
                     const std::string& status_ep) : 
            http_ctx_(http_ctx)
            , dbm_(std::make_shared<DBM>(username, password, db_name, mysql_addr, mysql_port))
            , redis_mgr_(std::make_shared<RedisMgr>(redis_ep))
            , status_rpc_(std::make_shared<StatusRPCClient>(status_ep))
            , handler_(std::make_shared<ReqHandler>(dbm_, redis_mgr_, status_rpc_))
            , http_(std::make_shared<HTTPServer>(http_ctx, http_ep, handler_))
            // , dbm_(std::make_shared<DBM>(username, password, db_name, mysql_addr, mysql_port))
            // , redis_mgr_(std::make_shared<RedisMgr>(redis_ep))
            // , http_(std::make_shared<HTTPServer>(http_ctx_, http_ep, dbm_))
            // , status_rpc_(std::make_unique<StatusRPCClient>(status_ep))
        {
            spdlog::info("GatewayClass initialized");
        }

        ~GatewayClass() {
            spdlog::info("GatewayClass terminating");
        }
    
        private:
        boost::asio::io_context& http_ctx_;
    
        // mysql connections manager
        std::shared_ptr<DBM> dbm_;
        // redis service object & manager
        std::shared_ptr<RedisMgr> redis_mgr_;
        // status service (远程RPC，需要状态服务器在线)
        std::shared_ptr<StatusRPCClient> status_rpc_;

        // 请求处理对象
        std::shared_ptr<ReqHandler> handler_;
        
        // HTTP server
        std::shared_ptr<HTTPServer> http_;
    
    };
}




#endif