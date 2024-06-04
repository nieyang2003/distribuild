#pragma once

#include <string>
#include <optional>

namespace distribuild {

std::string ZSTDCompress(const std::string& data);

std::optional<std::string> ZSTDDecompress(const std::string& data);

} // namespace distribuild