#pragma once

#include <string>
#include <optional>

#include "compiler_args.h"
#include "common/env_options.h"

namespace distribuild::client {
  
struct RewriteFileResult {
  CacheStatus cache_status;   // 是否允许缓存
  std::string language;       // 语言
  std::string source_path;    // 源路径
  std::string zstd_rewritten; // zstd压缩后的源码
  std::string source_digest;  // 
};

std::optional<RewriteFileResult> RewriteFile(const CompilerArgs& args);

}
