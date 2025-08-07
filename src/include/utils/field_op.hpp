#ifndef UTIL_FIELD_OP_HEADER
#define UTIL_FIELD_OP_HEADER

#include <cstring>
#include <netinet/in.h>
#include <endian.h>

inline uint64_t htonll(uint64_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
#else
    return value;
#endif
}

inline uint64_t ntohll(uint64_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
#else
    return value;
#endif
}

// @brief 向缓冲区某个偏移量写入32位的UINT值，并进行主机序->网络序的转换
// @param dst 缓冲区的写入起始偏移位置
// @param val 写入的值（主机序）
inline void WriteNetField32(char* dst, uint32_t val) {
    val = htonl(val);   // net long
    std::memcpy(dst, (void*)&val, sizeof(val));
}

// @brief 从缓冲区某个偏移量读取32位的UINT值，并进行网络序->主机序的转换
// @param beg 缓冲区的读取起始偏移位置
// @return 读取的值（主机序）
inline uint32_t ReadNetField32(const char* beg) {
    uint32_t val = 0;
    std::memcpy((void*)&val, beg, sizeof(val));
    return ntohl(val);
}

// @brief 向缓冲区某个偏移量写入16位的UINT值，并进行主机序->网络序的转换
// @param dst 缓冲区的写入起始偏移位置
// @param val 写入的值（主机序）
inline void WriteNetField16(char* dst, uint16_t val) {
    val = htons(val);   // net short
    std::memcpy(dst, (void*)&val, sizeof(val));
}

// @brief 从缓冲区某个偏移量读取16位的UINT值，并进行网络序->主机序的转换
// @param beg 缓冲区的读取起始偏移位置
// @return 读取的值（主机序）
inline uint16_t ReadNetField16(const char* beg) {
    uint16_t val = 0;
    std::memcpy((void*)&val, beg, sizeof(val));
    return ntohs(val);
}

// @brief 向缓冲区某个偏移量写入64位的UINT值，并进行主机序->网络序的转换
// @param dst 缓冲区的写入起始偏移位置
// @param val 写入的值（主机序）
inline void WriteNetField64(char* dst, uint64_t val) {
    val = htonll(val);   // net longlong
    std::memcpy(dst, (void*)&val, sizeof(val));
}

// @brief 从缓冲区某个偏移量读取64位的UINT值，并进行网络序->主机序的转换
// @param beg 缓冲区的读取起始偏移位置
// @return 读取的值（主机序）
inline uint64_t ReadNetField64(const char* beg) {
    uint64_t val = 0;
    std::memcpy((void*)&val, beg, sizeof(val));
    return ntohll(val);
}



#endif