#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace distribuild::client {

std::string MakeMultiChunkHeader(const std::vector<std::string_view>& parts);

} // namespace distribuild::client