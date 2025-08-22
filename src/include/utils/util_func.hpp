#ifndef UTIL_FUNCTIONS_HEADER
#define UTIL_FUNCTIONS_HEADER

#include <cstdint>
#include <ctime>
#include <random>

namespace chatroom {

    // @brief 获取当前时间戳，单位为毫秒
    inline uint64_t get_timestamp_ms() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
    }

    inline uint64_t RandomUInt64Generator() {
        static std::random_device rd;
        static std::mt19937 mt(rd());
        static std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        return dist(mt);
    }

}

#endif