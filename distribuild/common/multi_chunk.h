#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <optional>

namespace distribuild {

std::string MakeMultiChunkHeader(const std::vector<std::string_view>& parts);

std::string MakeMultiChunk(const std::vector<std::string_view>& parts);

std::string MakeMultiChunk(const std::vector<std::string>& parts);

std::optional<std::vector<std::string_view>> TryParseMultiChunk(const std::string_view& view);

} // namespace distribuild