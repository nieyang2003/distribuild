#pragma once

#include <optional>
#include <string>
#include <chrono>

#include "distribuild/daemon/cache.h"
#include "distribuild/daemon/bloom_filter.h"

#include "cache.grpc.pb.h"
#include "cache.pb.h"

namespace distribuild::daemon::local {

class CacheReader {
 public:
  static CacheReader* Instance();

  CacheReader();
  ~CacheReader();

  std::optional<CacheEntry> TryRead(const std::string& key);

 private:
  void OnTimerLoadBloomFilter();

 private:
  std::unique_ptr<cache::CacheService::Stub> stub_;

  std::chrono::steady_clock::time_point last_bf_update_;
  std::chrono::steady_clock::time_point last_bf_full_update_;

  std::mutex  bf_mutex_;
  BloomFilter bloom_filter_;
};

} // namespace distribuild::daemon::local