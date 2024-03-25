/**
 * @file disk_cache.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 磁盘缓存
 * @version 0.1
 * @date 2024-03-25
 * 
 * 
 */
#ifndef __DISTBU_CACHE_DISK_CACHE_H__
#define __DISTBU_CACHE_DISK_CACHE_H__
#include "cache_interface.h"

namespace distbu {

class Buffer;

class DiskCache : public CacheInterface {
public:
    DiskCache();

    Buffer Get() override;
    void Put(const std::string& key, const Buffer& buf) override;
    void Clear() override;

    // ? 改用别的
    std::vector<std::string> GetKeys() override;

private:

};

} // namespace distribuild

#endif
