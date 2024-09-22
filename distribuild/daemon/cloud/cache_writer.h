#pragma once
#include <future>
#include <memory>
#include <optional>
#include <Poco/TaskManager.h>
#include "daemon/cache.h"
#include "../build/distribuild/proto/cache.grpc.pb.h"
#include "../build/distribuild/proto/cache.pb.h"

namespace distribuild::daemon::cloud {

class CacheWriter {
 public:
  static CacheWriter* Instance();

  CacheWriter();

  ~CacheWriter();

  // 异步写入缓存
  std::optional<std::future<bool>> AsyncWrite(const std::string& key, CacheEntry&& cache_entry);

 private:
  std::unique_ptr<cache::CacheService::Stub> stub_;
  Poco::TaskManager task_manager_;
};

} // namespace distribuild::daemon::cloud