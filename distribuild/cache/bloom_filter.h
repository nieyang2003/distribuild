#pragma once
#include <string>

namespace distribuild::cache {

class BloomFilter {
 public:
  void Add(const std::string& cache_key);
};

} // namespace distribuild::cache