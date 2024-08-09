
#pragma once
#include <string>
#include <unordered_set>
#include <memory>
#include "common/logging.h"
#include "common/tools.h"

namespace distribuild {

class TokenVerifier {
 public:
  TokenVerifier() = default;

  explicit TokenVerifier(std::unordered_set<std::string> recognized_tokens)
    : recognized_tokens_(std::move(recognized_tokens)) {
    if (recognized_tokens_.count("") != 0) {
  	  LOG_WARN("存在空令牌，可能导致安全漏洞");
    }
    if (recognized_tokens_.empty()) {
  	  LOG_ERROR("必须提供至少一个令牌");
    }
  }
  
  bool Verify(const std::string& token) const noexcept {
	return recognized_tokens_.count(token) != 0;
  }

 private:
  std::unordered_set<std::string> recognized_tokens_;
};

// ============================================================================= //

inline std::unique_ptr<TokenVerifier> MakeTokenVerifier(const std::string& str) {
  DISTBU_CHECK_FORMAT(!str.empty(), "至少提供一个令牌");
  auto tokens = Split(str, ',', true /* keep_empty */);

  std::unordered_set<std::string> translated;
  for (auto&& e : tokens) {
    translated.insert(std::string(e));
  }
  return std::make_unique<TokenVerifier>(translated);
}

} // namespace distribuild