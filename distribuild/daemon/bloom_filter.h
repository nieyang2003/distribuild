#pragma once

#include <string>

namespace distribuild::daemon {

class BloomFilter {
 public:
  void Add(const std::string& key);
  bool PossiblyContains(const std::string& key);
 private:

};

} // namespace distribuild::daemon