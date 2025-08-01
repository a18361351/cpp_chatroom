#ifndef HTTP_GATEWAY_CLASS_HEADER
#define HTTP_GATEWAY_CLASS_HEADER

// gateway_class: 负责实际进行网关的职责：登陆验证+负载均衡
//  登陆验证中，我们使用了OpenSSL加密套件中的PBKDF2加密

#include <boost/asio/io_context.hpp>

#include "utils/util_class.hpp"
#include "http/dbm/gateway_dbm.hpp"
#include "http/http_server.hpp"
#include "http/gateway_rpc.hpp"

const std::string mysql_addr = "192.168.56.101";
const int mysql_port = 3306;

const std::string username = "lance";
const std::string password = "123456";
const std::string db_name = "chat";

const std::string status_srv = "192.168.56.101:3000";

// TODO(user): namespace

namespace chatroom::gateway {

    
    
    // 网关服务器的一个实例
    class GatewayClass : public Noncopyable /* 或者GatewayApp */ {
        public:
        void run(uint db_conn);
        GatewayClass(boost::asio::io_context& http_ctx, const boost::asio::ip::tcp::endpoint& http_ep) : http_ctx_(http_ctx)
            , dbm_(std::make_shared<DBM>(username, password, db_name, mysql_addr, mysql_port))
            , http_(std::make_shared<HTTPServer>(http_ctx_, http_ep, dbm_))
            , status_rpc_(std::make_unique<StatusRPCClient>(status_srv))
        {
    
        }
    
        private:
        boost::asio::io_context& http_ctx_;
    
        // mysql connections manager
        std::shared_ptr<DBM> dbm_;
    
        // HTTP server
        std::shared_ptr<HTTPServer> http_;
    
        // status service (远程RPC，需要状态服务器在线)
        std::unique_ptr<StatusRPCClient> status_rpc_;
    
    };
}




#endif