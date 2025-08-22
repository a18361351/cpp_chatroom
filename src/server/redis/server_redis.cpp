#include "server/redis/server_redis.hpp"

#include <absl/strings/numbers.h>
#include <sw/redis++/errors.h>

#include <chrono>
#include <iterator>

#include "log/log_manager.hpp"

namespace chatroom::backend {
std::optional<uint64_t> RedisMgr::VerifyUser(std::string_view token) {
    std::string key("token:");
    key += token;
    auto ret = GetRedis().get(key);
    if (ret == nullopt) {
        return nullopt;
    }
    uint64_t uid = std::stoull(ret.value());
    return uid;
}

std::optional<std::string> RedisMgr::GetUserLocation(uint64_t uid) {
    std::string key = "status:";
    key += std::to_string(uid);
    auto ret = GetRedis().hget(key, "server_id");
    return ret;
}

std::string RedisMgr::SendToMsgQueue(std::string_view server_id, uint64_t from, uint64_t to, std::string_view content,
                                     int queue_max_len) {
    std::string mq_key = "stream:server:";
    mq_key += server_id;
    std::string from_str = std::to_string(from);
    std::string to_str = std::to_string(to);
    std::vector<std::pair<std::string_view, std::string_view>> msg = {
        {"from", from_str}, {"to", to_str}, {"content", content}};
    // approx = true
    return GetRedis().xadd(mq_key, "*", msg.begin(), msg.end(), queue_max_len, true);
}

bool RedisMgr::UpdateUserStatus(std::string_view server_id, uint64_t uid) {
    std::string key_name = "status:";
    key_name += std::to_string(uid);
    thread_local std::unordered_map<string, string> user_data = {{"server_id", "unset"}, {"status", "online"}};
    user_data["server_id"] = server_id;
    auto ans = GetRedis().hsetex(key_name, user_data.begin(), user_data.end(), std::chrono::seconds(30),
                                 sw::redis::HSetExOption::ALWAYS);
    return ans == 1;
}

void RedisMgr::RecvFromMsgQueueNoACK(std::string_view server_id, std::string_view consumer_id,
                                     std::unordered_map<std::string, ItemStream> &out, uint block_ms, uint recv_count) {
    std::string mq_key = "stream:server:";
    mq_key += server_id;
    std::string mq2_key = "stream:serverctl:";
    mq2_key += server_id;
    std::vector<std::pair<std::string, std::string>> mqs_name = {{mq_key, ">"}, {mq2_key, ">"}};
    std::string consumer = "server";
    consumer += consumer_id;
    std::string group_name = "message_group";
    group_name += server_id;
    // GetRedis().xread(mq_key, "$", std::chrono::milliseconds(block_ms), recv_count, std::back_inserter(out));
    // noack = true
    // GetRedis().xreadgroup(group_name, consumer, mq_key, ">", std::chrono::milliseconds(block_ms), recv_count, true,
    // std::inserter(out, out.end()));
    GetRedis().xreadgroup(group_name, consumer, mqs_name.begin(), mqs_name.end(), recv_count, true,
                          std::inserter(out, out.end()));
}

void RedisMgr::RegisterMsgQueue(std::string_view server_id, bool read_from_begin) {
    std::string mq_key = "stream:server:";
    mq_key += server_id;
    std::string mq2_key = "stream:serverctl:";
    mq2_key += server_id;
    std::string group_name = "message_group";
    group_name += server_id;
    try {
        GetRedis().xgroup_create(mq_key, group_name, read_from_begin ? "0" : "$", true);
    } catch (const sw::redis::ReplyError &e) {
        spdlog::info("Message queue group {} already exists, ignoring error: {}", group_name, e.what());
    }
    try {
        GetRedis().xgroup_create(mq2_key, group_name, read_from_begin ? "0" : "$", true);
    } catch (const sw::redis::ReplyError &e) {
        spdlog::info("Message queue group {} already exists, ignoring error: {}", group_name, e.what());
    }
}

}  // namespace chatroom::backend