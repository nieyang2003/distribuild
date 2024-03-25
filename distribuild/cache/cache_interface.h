/**
 * @file cache_interface.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 缓存接口定义
 * @version 0.1
 * @date 2024-03-25
 * 
 * 
 */
#ifndef __DISTBU_CACHE_CACHE_INTERFACE_H__
#define __DISTBU_CACHE_CACHE_INTERFACE_H__
#include <string>
#include <vector>

namespace distbu {

class Buffer;

class CacheInterface {
public:
    virtual ~CacheInterface() = default;

    virtual Buffer Get() = 0;
    virtual void Put(const std::string& key, const Buffer& buf) = 0;
    virtual void Clear() = 0;

    // ? 改用别的
    virtual std::vector<std::string> GetKeys() = 0;
};

} // namespace distbu

#endif
