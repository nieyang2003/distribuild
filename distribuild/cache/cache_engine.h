#pragma once
#include <optional>
#include <string>
#include <vector>
#include <optional>

namespace distribuild::cache {

class CacheEngine {
public:
  virtual ~CacheEngine() = default;
  virtual std::vector<std::string> GetKeys() = 0;
  virtual std::optional<std::string> TryGet(const std::string& key) = 0;
  virtual void Put(const std::string& key, const std::string& bytes) = 0;
  virtual void Purge() = 0;
};

} // namespace distribuild::cache
