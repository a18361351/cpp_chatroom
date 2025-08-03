#include <boost/algorithm/hex.hpp>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "http/dbm/security.hpp"

std::string Security::HashPassword(std::string_view code, int iter) {
    // 生成盐值
    std::array<unsigned char, 16> salt;
    assert(RAND_bytes(salt.data(), salt.size()) == 1);

    // PBKDF派生密钥
    std::array<unsigned char, 64> derived_key;
    PKCS5_PBKDF2_HMAC(code.data(), code.size(),     // NOLINT
                        salt.data(), salt.size(),
                    iter, EVP_sha512(),
                derived_key.size(), derived_key.data());
    
    return std::to_string(iter) + "&" + 
            Bin2Hex(derived_key.data(), derived_key.size()) + "&" +
            Bin2Hex(salt.data(), salt.size());
}
bool Security::Verify(std::string_view code, std::string_view stored_hash) {
    // 1. 解析存储的哈希值
    size_t pos1 = stored_hash.find('&');
    size_t pos2 = stored_hash.find('&', pos1 + 1);
    
    if (pos1 == std::string::npos || pos2 == std::string::npos) {
        return false; // 无效格式
    }
    
    int iter = std::stoi(std::string(stored_hash.substr(0, pos1)));
    std::string key_hex = std::string(stored_hash.substr(pos1 + 1, pos2 - pos1 - 1));
    std::string salt_hex = std::string(stored_hash.substr(pos2 + 1));
    
    // 2. 十六进制转二进制
    auto salt = Hex2Bin(salt_hex);
    auto stored_key = Hex2Bin(key_hex);

    // 3. 使用相同参数重新计算
    std::vector<unsigned char> derived_key(stored_key.size());

    PKCS5_PBKDF2_HMAC(code.data(), code.size(),     // NOLINT
                        salt.data(), salt.size(),   // NOLINT
                    iter, EVP_sha512(),
                derived_key.size(), derived_key.data());    // NOLINT

    // 4. 恒定时间的安全比较
    return CRYPTO_memcmp(derived_key.data(), stored_key.data(), derived_key.size()) == 0;
}

std::string Security::Bin2Hex(unsigned char* bin, uint len) {
    std::string hex;
    hex.reserve(len * 2);
    boost::algorithm::hex(bin, bin + len, std::back_inserter(hex));
    return hex;
}

std::vector<unsigned char> Security::Hex2Bin(std::string_view hex) {
    std::vector<unsigned char> bin;
    bin.reserve(hex.size() / 2);
    boost::algorithm::unhex(hex, std::back_inserter(bin));
    return bin;
}