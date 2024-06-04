#pragma once

#include <string>

namespace distribuild {

std::string Blake3(std::initializer_list<std::string_view> data);

} // namespace distribuild