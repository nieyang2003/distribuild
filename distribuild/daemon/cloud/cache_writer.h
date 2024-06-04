#pragma once

#include <future>
#include <memory>

#include "distribuild/daemon/cache.h"

namespace distribuild::daemon::cloud {

class CacheWriter {
 public:
  static CacheWriter* Instance();
  CacheWriter();
  ~CacheWriter();
 
  std::future<bool> AsyncWrite(const std::string& key, const CacheEntry& cache_entry);

 private:
  

};

} // namespace distribuild::daemon::cloud