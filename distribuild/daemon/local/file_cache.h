#pragma once

#include <optional>
#include <string>
#include <shared_mutex>
#include <unordered_map>

namespace distribuild::daemon::local {

class FileCache {
 public:
  static FileCache* Instance();

  FileCache();

  std::optional<std::string> TryGet(const std::string& path, std::uint64_t size, std::uint64_t mtime) const;
  void Set(const std::string& path, std::uint64_t size, std::uint64_t mtime, const std::string& hash);

 private:
  struct CacheDesc{
    std::uint64_t size;
	std::uint64_t mtime;
	std::string   hash;
  };

  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, CacheDesc> caches_;

};

} // namespace distribuild::daemon::local