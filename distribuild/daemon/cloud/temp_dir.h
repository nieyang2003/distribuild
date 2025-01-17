#pragma once

#include <string>
#include <vector>

namespace distribuild::daemon::cloud {

class TempDir {
 public:
  TempDir() : alive_(false) {}
  explicit TempDir(const std::string& prefix);
  ~TempDir();

  /// @brief 读取目录下包括递归子目录所有的文件
  /// @param subdir 子目录
  /// @return <文件路径，文件数据>数组
  std::vector<std::pair<std::string, std::string>> ReadAll(const std::string& subdir = "");

  /// @brief 删除自己
  void Clear();

  /// @brief 获取自己的路径
  std::string GetPath() const { return path_; }

 private:
  bool alive_; // 是否有效
  std::string path_;
};

/// @brief 获取缓存目录位置，用于生产缓存文件
/// @return 
const std::string& GetTempDir();

} // namespace distribuild::daemon::cloud