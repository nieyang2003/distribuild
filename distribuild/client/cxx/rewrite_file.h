#pragma once
#include "client/cxx/compiler_args.h"
#include "client/common/config.h"
#include <string>
#include <optional>

namespace distribuild::client {
  
struct RewriteResult {
  bool directives_only;        // 使用"-fdirectives-only"选项是否成功
  CacheControl cache_control;  // 是否允许缓存
  std::string  language;       // 源代码语言
  std::string  source_path;    // 源代码源路径
  std::string  zstd_rewritten; // zstd压缩后的源码
  std::string  source_digest;  // 源码blake3哈希后的摘要
};

std::optional<RewriteResult> RewriteFile(const CompilerArgs& args);

} // namespace distribuild::client
