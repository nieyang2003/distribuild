#include "multi_chunk.h"

namespace distribuild::client {

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

} // namespace distribuild::client