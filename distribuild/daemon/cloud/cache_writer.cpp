#include "cache_writer.h"

#include "distribuild/common/logging.h"

namespace distribuild::daemon::cloud {

CacheWriter* CacheWriter::Instance() {
  static CacheWriter instance;
  return &instance;
}

CacheWriter::CacheWriter() {
  // TODO: 远程配置模块获取缓存地址
  // TODO: 创建异步存根
}

CacheWriter::~CacheWriter() {}

std::future<bool> CacheWriter::AsyncWrite(const std::string& key, const CacheEntry& cache_entry) {
  LOG_TRACE("写入{}", key);
  return std::future<bool>();
}

} // namespace distribuild::daemon::cloud