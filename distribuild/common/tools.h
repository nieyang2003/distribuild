#pragma once
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <sstream>
#include <optional>
#include "common/logging.h"

using namespace std::literals;

namespace distribuild {

inline bool StartWith(const std::string_view& s, const std::string_view& pattern) {
  return s.size() >= pattern.size() && s.substr(0, pattern.size()) == pattern;
}

// ============================================================================= //

inline bool EndWith(const std::string_view& s, const std::string_view& pattern) {
  return s.size() >= pattern.size() && s.substr(s.size() - pattern.size()) == pattern;
}

// ============================================================================= //

inline std::vector<std::string_view> Split(std::string_view s, std::string_view delim, bool keep_empty) {
  std::vector<std::string_view> splited;
  if (s.empty()) {
	return splited;
  }
  DISTBU_CHECK(!delim.empty());

  auto cur = s;
  while (true) {
    auto pos = cur.find(delim);
	if (pos != 0 || keep_empty) {
	  splited.push_back(cur.substr(0, pos));
	}
	if (pos == std::string_view::npos) {
	  break;
	}
	cur = cur.substr(pos + delim.size());
	if (cur.empty()) {
	  if (keep_empty) {
		splited.push_back("");
	  }
	  break;
	}
  }

  return splited;
}

inline std::vector<std::string_view> Split(std::string_view s, char delim, bool keep_empty) {
  return Split(s, std::string_view(&delim, 1), keep_empty);
}

// ============================================================================= //

inline std::string EscapeCommandArgument(const std::string_view &str) {
  std::string result;
  for (size_t i = 0; i < str.size(); ++i) {
    switch (str[i]) {
      case '\a':
        result += "\\a";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      case '\v':
        result += "\\v";
        break;
      case ' ':
      case '>':
      case '<':
      case '!':
      case '"':
      case '#':
      case '$':
      case '&':
      case '(':
      case ')':
      case '*':
      case ',':
      case ':':
      case ';':
      case '?':
      case '@':
      case '[':
      case '\\':
      case ']':
      case '`':
      case '{':
      case '}':
        result += '\\';
        [[fallthrough]];
      default:
        result += str[i];
        break;
    }
  }
  return result;
}

// ============================================================================= //

inline void SetTimeout(grpc::ClientContext* context, const std::chrono::seconds& sec) {
  auto deadline = gpr_time_add(
    gpr_now(GPR_CLOCK_REALTIME),
    gpr_time_from_seconds(sec / 1s, GPR_TIMESPAN)
  );
  context->set_deadline(deadline);
}

// ============================================================================= //

/// @brief 多个文件打包为一个文件
/// @param files 
/// @return 
inline std::string PackFiles(std::vector<std::pair<std::string, std::string>> files) {
  std::string suffix_line; // 第一行文件扩展名
  std::string size_line;   // 第二行size
  std::string result;
  std::size_t files_size = 0;

  // 判空
  if (files.empty()) return result;
  // 写入格式行
  for (auto&& [suffix, file] : files) {
	suffix_line.append(suffix);
	suffix_line.push_back(',');
	size_line.append(std::to_string(file.size()));
	size_line.push_back(',');

	files_size += file.size();
  }
  suffix_line.pop_back();
  suffix_line.push_back('\n');
  size_line.pop_back();
  size_line.push_back('\n');
  // 写入结果
  result.append(std::to_string(files_size));
  result.push_back('\n');
  result.append(suffix_line);
  result.append(size_line);
  for (auto&& [_, file] : files) {
	result.append(file);
  }

  return result;
}

/// @brief 解压打包后的文件
/// @param packed_file 
/// @return 
inline std::optional<std::vector<std::pair<std::string, std::string>>>
TryUnpackFiles(const std::string& packed_file) {
  std::vector<std::pair<std::string, std::string>> result;
  std::stringstream ss(packed_file);
  std::size_t head_size = 0;
  std::size_t files_size = 0;

  // 解析大小
  std::string length_line;
  std::getline(ss, length_line);
  head_size += length_line.size() + 1;
  files_size = std::stol(length_line);

  // 解析前缀格式行
  std::string suffixs_line;
  std::getline(ss, suffixs_line);
  head_size += suffixs_line.size() + 1;
  auto suffixs = Split(suffixs_line, ',', false);

  // 解析文件大小行
  std::string sizes_line;
  std::getline(ss, sizes_line);
  head_size += sizes_line.size() + 1;
  auto sizes = Split(sizes_line, ',', false);

  if (suffixs.size() != sizes.size()) {
	return std::nullopt;
  }
  if ((packed_file.size() - head_size) != files_size) {
	return std::nullopt;
  }
  
  size_t pos = head_size;
  for (size_t i = 0; i < sizes.size(); ++i) {
	size_t len = std::stoul(std::string(sizes[i]));
	result.push_back({std::string(suffixs[i]), packed_file.substr(pos, len)});
	pos += len;
  }

  return result;
}

// ============================================================================= //

namespace {

static const std::string kAttachmentKey = "attachment";

} // namespace

/// @brief 读取grpc server context 中的附加文件
/// @return 
inline std::string GetAttachment(const grpc::ServerContext* context) {
  auto it = context->client_metadata().find(kAttachmentKey);
  if (it != context->client_metadata().end()) {
      return std::string(it->second.data(), it->second.length());
  } else {
      // 没有找到附件，返回空的 std::optional
	  LOG_ERROR("获取 attachment 失败");
      return {};
  }
}

inline std::string GetAttachment(const grpc::ClientContext* context) {
  auto server_metadata = context->GetServerTrailingMetadata();
  auto it = server_metadata.find(kAttachmentKey);
  if (it != server_metadata.end()) {
      return std::string(it->second.data(), it->second.length());
  } else {
      // 没有找到附件，返回空的 std::optional
	  LOG_ERROR("获取 attachment 失败");
      return {};
  }
}

/// @brief 设置grpc附件文件
/// @param context 
/// @param data 
inline void SetAttachment(grpc::ServerContext* context, const std::string& data) {
  context->AddInitialMetadata(kAttachmentKey, data);
}

inline void SetAttachment(grpc::ClientContext* context, const std::string& data) {
    context->AddMetadata(kAttachmentKey, data);
}

} // namespace distribuild