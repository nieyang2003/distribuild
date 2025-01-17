#pragma once
#include <string>
#include <blake3.h>

namespace {

template<class F>
std::string Blake3Impl(F&& cb) {
  blake3_hasher state;
  blake3_hasher_init(&state);
  std::forward<F>(cb)(&state);
  std::uint8_t output[BLAKE3_OUT_LEN];
  blake3_hasher_finalize(&state, output, BLAKE3_OUT_LEN);
  return std::string(reinterpret_cast<char*>(output), BLAKE3_OUT_LEN);
}

} // namespace

namespace distribuild {

inline std::string Blake3(std::initializer_list<std::string_view> data) {
  return Blake3Impl([&](auto* state) {
    for (auto&& e : data) {
	  blake3_hasher_update(state, e.data(), e.size());
	}
  });
}

inline std::string Blake3(const std::string_view& data) {
  return Blake3Impl([&](auto* state) {
    blake3_hasher_update(state, data.data(), data.size());
  });
}

} // namespace distribuild