#pragma

#include <string_view>

namespace distribuild {

struct XxHash {
  std::size_t operator()(const std::string_view& str) const noexcept;
};

}