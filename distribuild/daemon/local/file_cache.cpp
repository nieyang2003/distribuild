#include "file_cache.h"

namespace distribuild::daemon::local {

FileCache* FileCache::Instance() {
  static FileCache instance;
  return &instance;
}

FileCache::FileCache() {}

std::optional<std::string> FileCache::TryGet(const std::string& path, std::uint64_t size, std::uint64_t mtime) const {
  std::shared_lock lock(mutex_);
  if (auto iter = caches_.find(path);
      iter != caches_.end() &&
	  iter->second.size == size &&
	  iter->second.mtime == mtime) {
	return iter->second.hash;
  }
  return std::nullopt;
}

void FileCache::Set(const std::string& path, std::uint64_t size, std::uint64_t mtime, const std::string& hash) {
  std::shared_lock lock(mutex_);
  caches_[path] = CacheDesc {
	.size = size,
	.mtime = mtime,
	.hash = hash,
  };
}

} // namespace distribuild::daemon::local