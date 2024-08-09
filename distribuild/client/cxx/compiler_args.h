#pragma once
#include "client/common/rewritten_args.h"
#include <string>
#include <vector>
#include <span>
#include <unordered_set>

namespace distribuild::client {

class CompilerArgs {
 public:
  using OptionArgs = std::span<const char*>;

  /// @brief 构造函数，解析编译器参数
  CompilerArgs(int argc, const char** argv);

  /// @brief 尝试查找编译选项并返回
  const OptionArgs* TryGet(const std::string_view& key) const;

  // 获得目标文件路径名
  std::string GetOutputFile() const;

  /// @brief 重写编译命令（增删相应选项）
  /// @param remove 匹配到整个选项直接移除
  /// @param remove_prefix 匹配到前缀则移除
  /// @param add 要添加的选项
  /// @param keep_filenames 保持文件在选项中
  /// @return 
  RewrittenArgs Rewrite(const std::unordered_set<std::string_view>& remove,
                        const std::vector<std::string_view>& remove_prefix,
						const std::initializer_list<std::string_view>& add,
						bool keep_filenames) const;
  
  /// @brief 返回命令行
  /// @return 
  std::string RebuiltArg() const { return compiler_ + " " + rebuilt_arg_; }

  const char* GetCompiler() const noexcept { return compiler_.c_str(); }
  void SetCompiler(std::string path) noexcept { compiler_ = std::move(path); }
  const std::vector<const char*>& GetFilenames() const noexcept { return filenames_; }
  

 private:
  std::string compiler_;
  std::vector<std::pair<const char*, OptionArgs>> args_;
  std::vector<const char*> filenames_;
  std::string rebuilt_arg_;  // 重建的命令行
};

} // namespace distribuild::client
