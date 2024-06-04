#pragma once

#include <string>

namespace distribuild::daemon::cloud {

// TODO: 使用更高效的IO方式

class TempFile {
 public:
  TempFile() = default;
  explicit TempFile(const std::string& temp_dir);
  ~TempFile();

  /// @brief 写入数据
  void Write(const std::string& data);

  /// @brief 读出数据
  std::string ReadAll() const;

  void Close();

  int Fd() const noexcept { return fd_; }
  const std::string& Path() const noexcept { return path_; }

 private:
  int fd_;
  std::string path_;
};

} // namespace distribuild::daemon::cloud