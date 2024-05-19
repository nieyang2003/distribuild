#pragma once

#include <string>
#include <optional>

#include "compiler_args.h"
#include "common/config.h"

namespace distribuild::client {
  
struct RewriteResult {
  bool directives_only;        // 使用"-fdirectives-only"选项是否成功
  CacheControl cache_control;  // 是否允许缓存
  std::string  language;       // 语言
  std::string  source_path;    // 源路径
  std::string  zstd_rewritten; // zstd压缩后的源码
  std::string  source_digest;  // 
};

std::optional<RewriteResult> RewriteFile(const CompilerArgs& args);

} // namespace distribuild::client
