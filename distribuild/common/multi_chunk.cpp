#include "multi_chunk.h"

#include "distribuild/common/string.h"
#include <limits.h>

namespace distribuild {

// 生成一个包含所有数据块大小信息的头部字符串，每个大小之间用逗号分隔，最后以回车换行符（\r\n）结束
std::string MakeMultiChunkHeader(const std::vector<std::string_view>& parts) {
  if (parts.empty()) {
	return {};
  }

  std::string result;
  for (auto&& e : parts) {
	result += std::to_string(e.size()) + ",";
  }
  result.pop_back();

  result += "\r\n";
  return result;
}

std::string MakeMultiChunk(const std::vector<std::string_view>& parts) {
  if (parts.empty()) {
	return {};
  }

  std::size_t total_size = 0;
  std::string result;
  for (auto&& e : parts) {
	result += std::to_string(e.size()) + ",";
	total_size += e.size();
  }
  result.pop_back();
  result += "\r\n";
  result.reserve(result.size() + total_size);
  for (auto&& e : parts) {
    result.append(e);
  }
  return result;
}

std::string MakeMultiChunk(const std::vector<std::string>& parts) {
  return MakeMultiChunk(std::vector<std::string_view>(parts.begin(), parts.end()));
}

std::optional<std::vector<std::string_view>> TryParseMultiChunk(const std::string_view& view) {
  std::vector<std::string_view> result;
  if (view.empty()) {
    return result;
  }

  auto delim = view.find('\n');
  if (delim == std::string_view::npos || delim == 0 ||
      view[delim - 1] != '\r') {
    return std::nullopt;
  }

  auto sizes = Split(view.substr(0, delim - 1), ",", false);
  auto rest_bytes = view.substr(delim + 1);
  std::vector<std::size_t> parsed_size;
  std::size_t total_size = 0;
  for (auto&& e : sizes) {
    auto str = std::string(e);
    auto size = strtol(str.data(), nullptr, 10);
    if (size <= 0 || size == LONG_MAX || size == LONG_MIN) {
      return std::nullopt;
    }
    parsed_size.push_back(size);
    total_size += size;
  }

  if (total_size != rest_bytes.size()) {
    return std::nullopt;
  }
  for (auto&& e : parsed_size) {
    result.push_back(rest_bytes.substr(0, e));
    rest_bytes.remove_prefix(e);
  }

  return result;
}

} // namespace distribuild