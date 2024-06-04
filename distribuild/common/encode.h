#pragma once

#include <string>

namespace distribuild {

std::string EncodeHex(std::string_view from, bool uppercase = false);

std::string EncodeHex(std::string_view from, std::string* to, bool uppercase = false);

}