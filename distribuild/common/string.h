#pragma once

#include <vector>
#include <string_view>

namespace distribuild {

bool StartWith(const std::string_view& s, const std::string_view& pattern) {
  return s.size() >= pattern.size() && s.substr(0, pattern.size()) == pattern;
}
bool EndWith(const std::string_view& s, const std::string_view& pattern) {
  return s.size() >= pattern.size() && s.substr(s.size() - pattern.size()) == pattern;
}

std::vector<std::string_view> Split(std::string_view s, std::string_view delim, bool keep_empty);

} // namespace distribuild