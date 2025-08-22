#ifndef HTTP_GATEWAY_CLASS_HEADER
#define HTTP_GATEWAY_CLASS_HEADER

// gateway_class: 负责实际进行网关的职责：登陆验证+负载均衡
//  登陆验证中，我们使用了OpenSSL加密套件中的PBKDF2加密

#include <sw/redis++/connection.h>
#include <sw/redis++/connection_pool.h>

#include <boost/asio/io_context.hpp>

#include "http/dbm/gateway_dbm.hpp"
#include "http/http_server.hpp"
#include "http/redis/gateway_redis.hpp"
#include "http/req_handler.hpp"
#include "http/rpc/status_rpc_client.hpp"
#include "utils/util_class.hpp"

namespace chatroom::gateway {
// MySQL服务器配置的类
struct DBConfigure {
    std::string mysql_addr_;
    uint mysql_port_;
    std::string username_;
    std::string password_;
    std::string db_name_;
    DBConfigure(std::string_view mysql_addr = "", uint mysql_port = 0, std::string_view username = "",
                std::string_view password = "", std::string_view db_name = "")
        : mysql_addr_(mysql_addr),
          mysql_port_(mysql_port),
          username_(username),
          password_(password),
          db_name_(db_name) {}
};

// 网关服务器的一个实例
class GatewayClass : public Noncopyable /* 或者GatewayApp */ {
   public:
    void Run(uint db_conn);
    explicit GatewayClass(boost::asio::io_context &http_ctx) : http_ctx_(http_ctx) {
        spdlog::debug("GatewayClass created");
    }

    void Initialize(const DBConfigure &db_conf, const boost::asio::ip::tcp::endpoint &http_ep,
                    const sw::redis::ConnectionOptions &redis_conn_opt,
                    const sw::redis::ConnectionPoolOptions &redis_pool_opt, const std::string &status_ep) {
        dbm_ = std::make_shared<DBM>(db_conf.username_, db_conf.password_, db_conf.db_name_, db_conf.mysql_addr_,
                                     db_conf.mysql_port_);
        redis_mgr_ = std::make_shared<RedisMgr>();
        redis_mgr_->ConnectTo(redis_conn_opt, redis_pool_opt);
        status_rpc_ = std::make_shared<StatusRPCClient>(status_ep);
        handler_ = std::make_shared<ReqHandler>(dbm_, redis_mgr_, status_rpc_);
        http_ = std::make_shared<HTTPServer>(http_ctx_, http_ep, handler_);
    }

    ~GatewayClass() { spdlog::info("GatewayClass terminating"); }

   private:
    boost::asio::io_context &http_ctx_;

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
}  // namespace chatroom::gateway

#endif