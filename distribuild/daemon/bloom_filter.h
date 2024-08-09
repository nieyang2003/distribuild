#pragma once

#include <string>

namespace distribuild::daemon {

class BloomFilter {
 public:
  void Add(const std::string& key) {}
  bool PossiblyContains(const std::string& key) { return false; }
 private:

};

} // namespace distribuild::daemon