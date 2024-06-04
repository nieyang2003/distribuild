#include "encode.h"

#include <array>

namespace distribuild {

namespace {

struct CharPair {
  char _1;
  char _2;
};

const auto kHexCharsLowercase = [] {
  char chars[] = "0123456789abcdef";
  std::array<CharPair, 256> result{};
  for (int i = 0; i != 256; ++i) {
	result[i]._1 = chars[(i >> 4) & 0xF];
	result[i]._2 = chars[i & 0xF];
  }
  return result;
}();

const auto kHexCharsUppercase = [] {
  char chars[] = "0123456789ABCDEF";
  std::array<CharPair, 256> result{};
  for (int i = 0; i != 256; ++i) {
	result[i]._1 = chars[(i >> 4) & 0xF];
	result[i]._2 = chars[i & 0xF];
  }
  return result;
}();

} // namespace

std::string EncodeHex(std::string_view from, bool uppercase = false) {
  std::string result;
  EncodeHex(from, &result, uppercase);
  return result;
}

std::string EncodeHex(std::string_view from, std::string* to, bool uppercase) {
  to->reserve(from.size() * 2);
  auto&& hex_chars = uppercase ? kHexCharsUppercase : kHexCharsLowercase;
  for (auto&& e : from) {
    auto i = static_cast<std::uint8_t>(e);
	to->append({hex_chars[i]._1, hex_chars[i]._2});
  }
}
}