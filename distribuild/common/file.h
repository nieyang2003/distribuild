/**
 * @file file.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 
 * @version 0.1
 * @date 2024-03-26
 * 
 * 
 */
#pragma once
#include <string>
#include "distribuild/common/nocopy.h"

namespace distribuild {

class File : NoCopy {
public:
    virtual ~File();
    const std::string& getPath() const { return m_path; }
    void write();
    void append();
private:
    int m_fd;
    std::string m_path;
};

class TempFile : public File {
public:
    TempFile();
    ~TempFile();
};

} // namespace distribuild