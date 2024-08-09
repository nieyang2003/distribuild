#include "client/common/rewritten_args.h"
#include "common/tools.h"

std::string distribuild::client::RewrittenArgs::ToString() const {
  std::string result;
  result += program_;
  for (auto&& e : args_) {
    result += " " + e;
  }
  return result;
}

std::string distribuild::client::RewrittenArgs::ToCommandLine(bool with_program) {
  std::string result;
  if (with_program) {
    result += program_ + " ";
  }
  for (auto&& e : args_) {
    result += EscapeCommandArgument(e) + " ";
  }
  if (!result.empty()) {
    result.pop_back();
  }
  return result;
}