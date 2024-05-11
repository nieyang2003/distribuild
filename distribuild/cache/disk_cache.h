/**
 * @file disk_cache.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 磁盘缓存
 * @version 0.1
 * @date 2024-03-25
 * 
 * 
 */
#pragma once
#include "cache.h"

namespace distribuild::cache {

class Buffer;

class DiskCache : public Cache {
public:
    DiskCache();

    Buffer get() override;
    void put(const std::string& key, const Buffer& buf) override;
    void clear() override;

    // ? 改用别的
    std::vector<std::string> getKeys() override;

private:

};

} // namespace distribuild::cache
