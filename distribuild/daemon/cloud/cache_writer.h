#pragma once
#include <future>
#include <memory>
#include "daemon/cache.h"
#include "../build/distribuild/proto/scheduler.grpc.pb.h"
#include "../build/distribuild/proto/scheduler.pb.h"

namespace distribuild::daemon::cloud {

class CacheWriter {
 public:
  static CacheWriter* Instance();
  CacheWriter();
  ~CacheWriter();

  // 异步写入缓存
  std::future<bool> AsyncWrite(const std::string& key, const CacheEntry& cache_entry);

 private:
  std::unique_ptr<scheduler::SchedulerService::Stub> stub_;
};

} // namespace distribuild::daemon::cloud