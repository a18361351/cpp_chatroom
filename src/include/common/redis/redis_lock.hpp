#ifndef COMMON_REDIS_LOCK_HEADER
#define COMMON_REDIS_LOCK_HEADER

#include <sw/redis++/command_options.h>
#include <sw/redis++/redis++.h>

#include <chrono>

#include "utils/util_func.hpp"

namespace chatroom {
// @brief 尝试获取一个Redis锁
// @param redis Redis对象
// @param lock_name 锁的名称，实现为Redis上对应锁对象的key值
// @param lock_timeout_ms 锁的过期时间，单位为毫秒
// @return 成功获取锁则返回锁的标识字符串，失败则返回空字符串
std::string AcquireTempLock(sw::redis::Redis &redis, std::string_view lock_name, uint lock_timeout_ms) {
    std::string key = "tmplock:";
    key += lock_name;
    // 生成一个纯随机的数，作为这把锁的持有者的标识
    std::string lock_info = std::to_string(RandomUInt64Generator()) + std::to_string(get_timestamp_ms());
    auto ans = redis.set(key, lock_info, std::chrono::milliseconds(lock_timeout_ms), sw::redis::UpdateType::NOT_EXIST);
    return (ans ? lock_info : std::string());
}

// @brief 释放一个Redis锁
// @param redis Redis对象
// @param lock_name 锁的名称
// @param lock_info 获取锁时返回的锁标识字符串
// @return 成功释放锁则返回true，失败则返回false
bool ReleaseTempLock(sw::redis::Redis &redis, std::string_view lock_name, std::string_view lock_info) {
    static std::string release_script =
        "if redis.call('get', KEYS[1]) == ARGV[1] then\n"
        "    return redis.call('del', KEYS[1])\n"
        "else\n"
        "    return 0\n"
        "end";
    static std::string script_hash;
    if (script_hash.empty()) {
        script_hash = redis.script_load(release_script);
    }

    std::string key = "tmplock:";
    key += lock_name;
    auto ans = redis.evalsha<long long>(script_hash, {key}, {lock_info});
    if (ans == 0) {
        return false;
    }
    return true;
}
}  // namespace chatroom

#endif