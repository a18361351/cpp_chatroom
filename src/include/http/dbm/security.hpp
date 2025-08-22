#ifndef HTTP_GATEWAY_SECURITY
#define HTTP_GATEWAY_SECURITY

#include <sys/types.h>

#include <string_view>
#include <vector>

// 明文密码生成哈希值，以及比对明文密码和哈希值是否匹配的函数
class Security {
   public:
    static std::string HashPassword(std::string_view code, int iter = 200000);
    static bool Verify(std::string_view code, std::string_view stored_hash);

   private:
    static std::string Bin2Hex(unsigned char *bin, uint len);
    static std::vector<unsigned char> Hex2Bin(std::string_view hex);
};

#endif