#pragma once
#include <optional>
#include <string>
#include <chrono>
#include <Poco/Timer.h>
#include "daemon/cache.h"
#include "daemon/bloom_filter.h"
#include "../build/distribuild/proto/cache.grpc.pb.h"
#include "../build/distribuild/proto/cache.pb.h"

namespace distribuild::daemon::local {

/// @brief 从编译缓存中读取文件
class CacheReader {
 public:
  static CacheReader* Instance();

  CacheReader();
  ~CacheReader();

  std::optional<CacheEntry> TryRead(const std::string& key);

 private:
  /// @brief 定时器函数，刷新布隆过滤器
  void OnTimerLoadBloomFilter(Poco::Timer& timer);

 private:
  std::unique_ptr<cache::CacheService::Stub> stub_;
  Poco::Timer timer_;
  std::chrono::steady_clock::time_point last_bf_update_;      // 最近的布隆过滤器增量更新时间
  std::chrono::steady_clock::time_point last_bf_full_update_; // 最近的布隆过滤器全量更新时间
  std::mutex  bf_mutex_;
  BloomFilter bloom_filter_;
};

} // namespace distribuild::daemon::local