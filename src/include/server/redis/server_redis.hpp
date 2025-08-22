#ifndef SERVER_REDIS_HEADER
#define SERVER_REDIS_HEADER

#include <sw/redis++/pipeline.h>
#include <sw/redis++/queued_redis.h>

#include <string_view>

#include "common/redis/base_redis_mgr.hpp"

namespace chatroom::backend {
class RedisMgr : public chatroom::BaseRedisMgr {
   public:
    // ctors
    RedisMgr() = default;

    // dtors
    ~RedisMgr() override = default;

    // Pipeline
    sw::redis::Pipeline GetPipeline() { return std::move(GetRedis().pipeline(false)); }

    std::optional<uint64_t> VerifyUser(std::string_view token);

    std::optional<std::string> GetUserLocation(uint64_t uid);

    std::string SendToMsgQueue(std::string_view server_id, uint64_t from, uint64_t to, std::string_view content,
                               int max_count = 1000);

    bool UpdateUserStatus(std::string_view server_id, uint64_t uid);

    using Attrs = std::unordered_map<std::string, std::string>;  // Item中的属性列表（键值对）
    using Item = std::pair<std::string, std::optional<Attrs>>;   // (id, attrs)
    using ItemStream = std::vector<Item>;  // 一个RedisStream流，包含其中的一系列消息

    // @brief 从消息队列中接收消息，接收即确认
    void RecvFromMsgQueueNoACK(std::string_view server_id, std::string_view consumer_id,
                               std::unordered_map<std::string, ItemStream> &out, uint block_ms = 2000,
                               uint recv_count = 10);

    void RegisterMsgQueue(std::string_view server_id, bool read_from_begin);

   private:
    std::unique_ptr<sw::redis::Pipeline> pl_;
};
}  // namespace chatroom::backend

#endif