#pragma once

#include <optional>
#include <string>
#include <vector>


namespace distribuild::cache {

class Buffer;

class Cache {
public:
  virtual ~Cache() = default;
  virtual std::vector<std::string> GetKeys() = 0;
  virtual std::optional<Buffer> TryGet(const std::string& key) = 0;
  virtual void Put(const std::string& key, const Buffer bytes) = 0;
  virtual void clear() = 0;
};

} // namespace distribuild::cache
