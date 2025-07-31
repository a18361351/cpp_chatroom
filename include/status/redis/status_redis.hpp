#ifndef HTTP_STATUS_REDIS_HEADER
#define HTTP_STATUS_REDIS_HEADER

// status_redis: 将redis对象和业务对象封装在一起，提供简化的接口
#include <iterator>
#include <stdexcept>
#include <sw/redis++/command_options.h>
#include <sw/redis++/redis.h>
#include <boost/asio/thread_pool.hpp>

constexpr const char* SERVER_LOAD = "server_load";
constexpr const char* SERVER_TABLE = "server_list";
constexpr const char* USER_TABLE = "user_list";

using namespace std;

// Redis类管理器
class StatusRedisMgr {
    private:
    // boost::asio::thread_pool pool_;
    std::shared_ptr<sw::redis::Redis> redis_;
    std::string sha_server_by_user;     // get server by userid
    std::string sha_min_load_server;    // get server with minimal load
    public:
    // 需要一个Redis对象
    explicit StatusRedisMgr(std::shared_ptr<sw::redis::Redis> redis_obj) : redis_(std::move(redis_obj)) {
        RegisterScript();
    }
    ~StatusRedisMgr() {
        UnregisterScript();
    }
    
    void RegisterScript() {
        const std::string get_server_by_userid = 
        "local server_id = redis.call(\"GET\", KEYS[1])\n"
        "if not server_id then\n"
        "   return nil\n"
        "end\n"
        "return redis.call(\"HGET\", " + std::string(SERVER_TABLE) + ", server_id)";
        sha_server_by_user = redis_->script_load(get_server_by_userid);

        const std::string get_minimal_load_server = 
        "local server_id = redis.call(\"ZRANGE\", " + std::string(SERVER_LOAD) +", 0, 0)\n"
        "if not server_id then\n"
        "   return nil\n"
        "end\n"
        "return redis.call(\"HGET\", " + std::string(SERVER_TABLE) + ", server_id)";
        sha_min_load_server = redis_->script_load(get_minimal_load_server);
    }

    void UnregisterScript() {
        redis_->script_flush();
    }


    // @brief 返回最小负载的服务器id
    std::optional<std::string> QueryMinimalLoadServerId() {
        vector<string> ans;
        redis_->zrange(SERVER_LOAD, 0, 0, std::back_inserter(ans));
        if (ans.empty()) return string();
        return ans.back();
    }

    // @brief 返回最小负载的服务器地址
    std::optional<std::string> QueryMinimalLoadServerAddr() {
        return redis_->evalsha<std::optional<std::string>>(sha_min_load_server, {}, {});
    }

    // @brief 根据用户id查询其所在的服务器id，如果用户未登录，返回nullopt
    std::optional<std::string> QueryServerIdByUser(std::string_view user_id) {
        return redis_->get(user_id);
    }

    // @brief 根据用户id查询其所在的服务器地址，如果用户未登录，返回nullopt
    std::optional<std::string> QueryServerAddrByUser(std::string_view user_id) {
        return redis_->evalsha<std::optional<std::string>>(sha_server_by_user, {user_id}, {});
    }

    void UpdateServerLoad(uint32_t server_id, uint32_t load) {
        throw std::logic_error("UpdateServerLoad() Not implemented");
    }
};

#endif