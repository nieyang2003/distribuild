/**
 * @file dir.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 目录操作
 * @version 0.1
 * @date 2024-03-25
 * 
 * 
 */
#pragma once
#include <sys/types.h>

#include <vector>
#include <string>


namespace distribuild {

void MkDir(const std::string& path, mode_t mode = 0755);
void RemoveDir(const std::string& path);

} // namespace distribuild

