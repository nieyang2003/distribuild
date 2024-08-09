#pragma once
#include <iostream>
#include <string>
#include <openssl/rsa.h>
#include <openssl/pem.h>

namespace distribuild {

// 使用 OpenSSL 生成 RSA 密钥对，并返回密钥对结构体
inline RSA* GenerateRSAKeyPair(int key_length) {
    RSA *rsa = RSA_new();
    BIGNUM *bignum = BN_new();
    BN_set_word(bignum, RSA_F4); // RSA_F4 是一个常用的 RSA 公钥指数
    RSA_generate_key_ex(rsa, key_length, bignum, NULL);
    BN_free(bignum);
    return rsa;
}

// 使用 OpenSSL 进行 RSA 加密
inline std::string RsaEncrypt(const std::string& plaintext, RSA* rsa_public_key) {
    int rsa_size = RSA_size(rsa_public_key);
    std::string ciphertext(rsa_size, 0);
    int encrypted_length = RSA_public_encrypt(plaintext.size(), reinterpret_cast<const unsigned char*>(plaintext.data()),
                                              reinterpret_cast<unsigned char*>(ciphertext.data()), rsa_public_key, RSA_PKCS1_PADDING);
    if (encrypted_length == -1) {
        // 加密失败
        return "";
    }
    ciphertext.resize(encrypted_length);
    return ciphertext;
}

// 使用 OpenSSL 进行 RSA 解密
inline std::string RsaDecrypt(const std::string& ciphertext, RSA* rsa_private_key) {
    int rsa_size = RSA_size(rsa_private_key);
    std::string decrypted(rsa_size, 0);
    int decrypted_length = RSA_private_decrypt(ciphertext.size(), reinterpret_cast<const unsigned char*>(ciphertext.data()),
                                               reinterpret_cast<unsigned char*>(decrypted.data()), rsa_private_key, RSA_PKCS1_PADDING);
    if (decrypted_length == -1) {
        // 解密失败
        return "";
    }
    decrypted.resize(decrypted_length);
    return decrypted;
}

} // namespace distribuild