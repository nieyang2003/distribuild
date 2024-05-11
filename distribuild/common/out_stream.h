/**
 * @file out_stream.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 
 * @version 0.1
 * @date 2024-03-26
 * 
 * 
 */
#pragma once
#include <string>

namespace distribuild {

class OutStream {
public:
    virtual ~OutStream() = default;
    virtual void write(const char* dest, std::size_t bytes) = 0;
};

class TransparentOutStream : public OutStream {
public:
    void write(const char* dest, std::size_t bytes) override;
private:
    std::string m_buffer;
};

} // namespace distribuild