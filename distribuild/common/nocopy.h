/**
 * @file nocopy.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief NoCopy
 * @version 0.1
 * @date 2024-03-26
 * 
 * 
 */
#pragma once
namespace distribuild {

class NoCopy {
public:
    NoCopy() = default;
    ~NoCopy() = default;
private:
    NoCopy(const NoCopy&) = delete;
    NoCopy(const NoCopy&&) = delete;
    NoCopy& operator==(const NoCopy&&) = delete;
};

} // namespace distribuild