#pragma once

#include <vector>
#include <string_view>

namespace distribuild {

bool StartWith(const std::string_view& s, const std::string_view& pattern);

bool EndWith(const std::string_view& s, const std::string_view& pattern);

std::vector<std::string_view> Split(std::string_view s, std::string_view delim, bool keep_empty);

} // namespace distribuild