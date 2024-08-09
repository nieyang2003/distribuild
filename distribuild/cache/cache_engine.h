#pragma once
#include <Poco/Buffer.h>
#include <optional>
#include <string>
#include <vector>

namespace distribuild::cache {

using Buffer = Poco::Buffer<char>;

class CacheEngine {
public:
  virtual ~CacheEngine() = default;
  virtual std::vector<std::string> GetKeys() = 0;
  virtual std::optional<Buffer> TryGet(const std::string& key) = 0;
  virtual void Put(const std::string& key, const Buffer bytes) = 0;
  virtual void Clear() = 0;
};

} // namespace distribuild::cache
