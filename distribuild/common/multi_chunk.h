#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <optional>
#include <limits.h>
#include "common/tools.h"

namespace distribuild {

/// @brief 生成一个包含所有数据块大小信息的头部字符串，每个大小之间用逗号分隔，最后以回车换行符（\r\n）结束
/// @param parts 
/// @return 
inline std::string MakeMultiChunkHeaderLine(const std::vector<std::string_view>& parts) {
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

// ============================================================================= //

/// @brief 制作分块数据，第一行为各个分块的数据大小
/// @param parts 
/// @return 
inline std::string MakeMultiChunk(const std::vector<std::string_view>& parts) {
  if (parts.empty()) {
	return {};
  }

  std::size_t total_size = 0;
  std::string result;

  for (auto&& e : parts) {
	result.append(std::to_string(e.size()));
	result.push_back(',');result.append(1, ',');
	total_size += e.size();
  }
  result.pop_back();
  result.append("\r\n");

  result.reserve(result.size() + total_size);
  for (auto&& e : parts) {
    result.append(e);
  }
  return result;
}

/// @brief 制作分块数据
/// @param parts 
/// @return 
inline std::string MakeMultiChunk(const std::vector<std::string>& parts) {
  return MakeMultiChunk(std::vector<std::string_view>(parts.begin(), parts.end()));
}

// ============================================================================= //

/// @brief 尝试解析分块
/// @param view 
/// @return 
inline std::optional<std::vector<std::string_view>> TryParseMultiChunk(const std::string_view& view) {
  std::vector<std::string_view> result;

  // 空，返回
  if (view.empty()) {
    return result;
  }

  // 寻找第一行
  auto delim = view.find('\n');
  if (delim == std::string_view::npos || delim == 0 || view[delim - 1] != '\r') {
    return std::nullopt;
  }

  // 从第一行解析出分块大小
  auto sizes = Split(view.substr(0, delim - 1), ',', false);
  auto rest_bytes = view.substr(delim + 1); // body
  std::vector<std::size_t> parsed_size;
  std::size_t total_size = 0;

  for (auto&& e : sizes) {
    auto size = strtol(std::string(e).c_str(), nullptr, 10);
    if (size <= 0 || size == LONG_MAX || size == LONG_MIN) {
      return std::nullopt;
    }
    parsed_size.push_back(size);
    total_size += size;
  }

  // 大小不同
  if (total_size != rest_bytes.size()) {
    return std::nullopt;
  }

  // 放入结果
  for (auto&& e : parsed_size) {
    result.push_back(rest_bytes.substr(0, e));
    rest_bytes.remove_prefix(e);
  }

  return result;
}

} // namespace distribuild