#pragma once
#include <iostream>
#include <openssl/rand.h>
#include <iomanip>

namespace distribuild {

// 生成128位AES密钥
inline std::string GenerateAESKey() {
    const int key_length = 16; // 128位密钥长度（以字节为单位）
    unsigned char key[key_length];
    RAND_bytes(key, key_length); // 使用OpenSSL的伪随机数生成器生成随机字节序列
    return std::string(reinterpret_cast<char*>(key), key_length);
}

} // namespace distribuild