#pragma once
#include <string>
#include <vector>

namespace distribuild::client {

class RewrittenArgs {
 public:
  /// @brief 构造函数
  /// @param program 程序名
  /// @param args 运行参数vector
  explicit RewrittenArgs(std::string program, std::vector<std::string> args)
    : program_(std::move(program)), args_(std::move(args)) {}

  /// @brief 获得程序名
  const std::string& GetProgram() const noexcept { return program_; }

  /// @brief 获得运行参数（无程序名）
  /// @return 
  const std::vector<std::string>& GetArgs() const noexcept { return args_; }

  /// @brief 将参数连接为一个string并返回
  /// @return 
  std::string ToString() const;

  /// @brief 将参数连接为一个处理过转义字符后的string返回
  /// @param with_program 
  /// @return 
  std::string ToCommandLine(bool with_program);

 private:
  std::string program_;
  std::vector<std::string> args_;
};

} // namespace distribuild::client