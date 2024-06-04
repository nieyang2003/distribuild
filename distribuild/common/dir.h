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

struct DirNode {
  std::string name;
  std::uint64_t inode;
  bool is_block_dev: 1;
  bool is_char_dev: 1;
  bool is_dir: 1;
  bool is_symlink: 1;
  bool is_regular: 1;
  bool is_unix_socket: 1;
};

void MkDir(const std::string& path, mode_t mode = 0755);
void RemoveDir(const std::string& path);

std::vector<DirNode> GetDirNodes(const std::string& path);
std::vector<DirNode> GetDirNodesRecursively(const std::string& path);

} // namespace distribuild

