/**
 * @file compiler_args.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 
 * @version 0.1
 * @date 2024-03-26
 * 
 * 
 */
#pragma once
#include <string>
#include <vector>
#include <span>

namespace distribuild::client {

class CompilerArgs {
 public:
  using OptionArgs = std::span<const char*>;

  /// @brief 解析编译器参数
  CompilerArgs(int argc, const char** argv);

  const char* GetCompiler() const noexcept { return compiler_.c_str(); }
  void SetCompiler(std::string path) noexcept { compiler_ = std::move(path); }
  std::string RebuiltArg() const { return compiler_ + " " + rebuilt_arg_; }

  // std::string GetObjectFile() const;
  RewriteResult

 private:
  std::string compiler_;
  std::vector<std::pair<const char*, OptionArgs>> args_;
  std::vector<const char*> filenames_;
  std::string rebuilt_arg_;  // 重建的命令行
};

} // namespace distribuild::client
