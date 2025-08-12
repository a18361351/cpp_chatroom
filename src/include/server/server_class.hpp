#ifndef SERVER_CLASS_HEADER
#define SERVER_CLASS_HEADER

// server_class.hpp: 后台服务器管理类ServerClass的声明头文件

#include <unordered_map>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <sw/redis++/connection.h>
#include <sw/redis++/connection_pool.h>

#include "server/io_context_pool.hpp"
#include "server/mq_handler.hpp"
#include "server/msg_handler.hpp"
#include "server/online_status_upload.hpp"
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
                    string server_addr,
                    boost::asio::io_context& listener_ctx, 
                    const std::string& status_rpc_addr,
                    const sw::redis::ConnectionOptions& redis_conn_opts,
                    const sw::redis::ConnectionPoolOptions& redis_pool_opts) : 
            server_id_(server_id)
            , server_addr_(std::move(server_addr))
            , ctx_(listener_ctx)
            , acc_(listener_ctx)
            , timer_mgr_(std::make_unique<TimerTaskManager>())
            , redis_(std::make_shared<RedisMgr>())
            , status_uploader_(std::make_shared<OnlineStatusUploader>(redis_, timer_mgr_.get()))
            , sess_mgr_(std::make_shared<SessionManager>())
            , handler_(std::make_shared<MsgHandler>(server_id_, sess_mgr_, redis_, status_uploader_))
            , rpc_cli_(std::make_shared<StatusRPCClient>(status_rpc_addr))
            , reporter_(std::make_shared<StatusReporter>(server_addr_, server_id_, rpc_cli_, sess_mgr_, timer_mgr_.get()))
            , mq_handler_()
        {
            // redis manager connect
            redis_->ConnectTo(redis_conn_opts, redis_pool_opts);

            // handler start
            handler_->Start();

            mq_handler_ = std::make_shared<MQHandler>(server_id_, sess_mgr_, redis_);

        }

        void Listen(const boost::asio::ip::tcp::endpoint& ep) {
            acc_.open(boost::asio::ip::tcp::v4());
            acc_.bind(ep);  // 
            acc_.listen();  // default backlog

            reporter_->Register();

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
            mq_handler_.reset();

            reporter_->Stop();
            reporter_.reset();
            handler_->Stop();
            handler_.reset();

            // 使用timer的类需要在timer_mgr_析构之前析构
            status_uploader_.reset();

            // timer worker stop
            timer_mgr_.reset();
        }
    private:
    
        uint32_t server_id_;
        std::string server_addr_;   // 使得外部主机能够连接到本服务器的地址（IP:Port）
        boost::asio::io_context& ctx_;
        boost::asio::ip::tcp::acceptor acc_;
        // 外部封装类
        // timer_mgr_类被StatusReporter类使用
        std::unique_ptr<TimerTaskManager> timer_mgr_;
        std::shared_ptr<RedisMgr> redis_;
        std::shared_ptr<OnlineStatusUploader> status_uploader_;
        std::shared_ptr<SessionManager> sess_mgr_;
        std::shared_ptr<MsgHandler> handler_;
        std::shared_ptr<StatusRPCClient> rpc_cli_;
        std::shared_ptr<StatusReporter> reporter_;
        std::shared_ptr<MQHandler> mq_handler_;
        
    };

}

#endif