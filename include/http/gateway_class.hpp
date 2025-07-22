#ifndef HTTP_GATEWAY_CLASS_HEADER
#define HTTP_GATEWAY_CLASS_HEADER

// gateway_class: 负责实际进行网关的职责：登陆验证+负载均衡
//  登陆验证中，我们使用了OpenSSL加密套件中的PBKDF2加密

#include <boost/asio/io_context.hpp>

#include "utils/util_class.hpp"
#include "http/dbm/gateway_dbm.hpp"
#include "http/http_server.hpp"

const std::string mysql_addr = "192.168.56.101";
const int mysql_port = 3306;

const std::string username = "lance";
const std::string password = "123456";
const std::string db_name = "chat";

// 网关服务器的一个实例
class GatewayClass : public Noncopyable /* 或者GatewayApp */ {
    public:
    void run(uint db_conn);
    GatewayClass(boost::asio::io_context& http_ctx, boost::asio::ip::tcp::endpoint http_ep) : http_ctx_(http_ctx)
        , dbm_(std::make_shared<DBM>(username, password, db_name, mysql_addr, mysql_port))
        , http_(std::make_shared<HTTPServer>(http_ctx_, http_ep, dbm_))
    {

    }

    private:
    boost::asio::io_context& http_ctx_;

    // mysql connections manager
    std::shared_ptr<DBM> dbm_;

    // HTTP server
    std::shared_ptr<HTTPServer> http_;

};



#endif