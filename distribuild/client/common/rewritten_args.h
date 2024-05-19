#pragma once

#include <string>
#include <vector>

namespace distribuild::client {

class RewrittenArgs {
 public:
  explicit RewrittenArgs(std::string program, std::vector<std::string> args)
    : program_(std::move(program)), args_(std::move(args)) {}

  const std::string& GetProgram() const noexcept { return program_; }
  const std::vector<std::string> GetArgs() const noexcept { return args_; }

  std::string ToString() const;

 private:
  std::string program_;
  std::vector<std::string> args_;
};

} // namespace distribuild::client