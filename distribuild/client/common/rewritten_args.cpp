#include "rewritten_args.h"

std::string distribuild::client::RewrittenArgs::ToString() const {
  std::string result;
  result += program_;
  for (auto&& e : args_) {
    result += " " + e;
  }
  return result;
}