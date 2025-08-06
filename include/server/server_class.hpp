#ifndef SERVER_CLASS_HEADER
#define SERVER_CLASS_HEADER

// server_class.hpp: 后台服务器管理类ServerClass的声明头文件

#include <sw/redis++/connection.h>
#include <sw/redis++/connection_pool.h>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include "server/io_context_pool.hpp"
#include "server/msg_handler.hpp"
#include "server/redis/server_redis.hpp"
#include "server/rpc/status_rpc_client.hpp"
#include "server/session_manager.hpp"
#include "server/status_reporter.hpp"

using errcode = boost::system::error_code;

namespace chatroom::backend {
    class ServerClass {
    public:
        // explicit ServerClass(boost::asio::io_context& listener_ctx) : context_(listener_ctx), acc_(listener_ctx) {}
        ServerClass(uint32_t server_id, 
                    boost::asio::io_context& listener_ctx, 
                    const std::string& status_rpc_addr,
                    const sw::redis::ConnectionOptions& redis_conn_opts,
                    const sw::redis::ConnectionPoolOptions& redis_pool_opts) : 
            server_id_(server_id)
            , ctx_(listener_ctx)
            , acc_(listener_ctx)
            , redis_(std::make_shared<RedisMgr>())
            , sess_mgr_(std::make_shared<SessionManager>())
            , handler_(std::make_shared<MsgHandler>(sess_mgr_, redis_))
            , rpc_cli_(std::make_shared<StatusRPCClient>(status_rpc_addr))
            , reporter_(std::make_shared<StatusReporter>(server_id, rpc_cli_, sess_mgr_)) 
        {
            // redis manager connect
            redis_->ConnectTo(redis_conn_opts, redis_pool_opts);

            // handler start
            handler_->Start();

        }

        void Listen(const boost::asio::ip::tcp::endpoint& ep) {
            acc_.open(boost::asio::ip::tcp::v4());
            acc_.bind(ep);  // 
            acc_.listen();  // default backlog

            AcceptorFn();
        }
    
        void AcceptorFn();
        
        // 强制下线逻辑
        bool Kick(const std::string& uuid);
    
        ~ServerClass() { // NOLINT
    #ifdef USING_IOCONTEXT_POOL
        Singleton<IOContextPool>::GetInstance().Stop();
    #elif defined(USING_IOTHREAD_POOL)
        Singleton<IOThreadPool>::GetInstance().Stop();
    #endif
            reporter_->Stop();
            handler_->Stop();
        }
    private:
    
    
        uint32_t server_id_;
        boost::asio::io_context& ctx_;
        boost::asio::ip::tcp::acceptor acc_;
        // 外部封装类
        std::shared_ptr<RedisMgr> redis_;
        std::shared_ptr<SessionManager> sess_mgr_;
        std::shared_ptr<MsgHandler> handler_;
        std::shared_ptr<StatusRPCClient> rpc_cli_;
        std::shared_ptr<StatusReporter> reporter_;
        
    };

}

#endif