/**
 * @file dir.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 目录操作
 * @version 0.1
 * @date 2024-03-25
 * 
 * 
 */
#ifndef __DISTRIBUILD_COMMON_DIR_H__
#define __DISTRIBUILD_COMMON_DIR_H__
#include <sys/types.h>

#include <vector>
#include <string>


namespace distbu {

void MkDir(const std::string& path, mode_t mode = 0755);
void RemoveDir(const std::string& path);

} // namespace distbu

#endif
