#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <zstd.h>

namespace distribuild {

inline std::optional<std::string> ZSTDCompress(const std::string_view& from) {
  size_t compressed_bound = ZSTD_compressBound(from.size()); // 计算压缩后的最大尺寸
  std::string compressed_data(compressed_bound, '\0');
  size_t compressed_size = ZSTD_compress(compressed_data.data(), compressed_bound, from.data(), from.size(), 1);
  if (ZSTD_isError(compressed_size)) {
    return std::nullopt;
  }
  compressed_data.resize(compressed_size);
  return compressed_data;
}

inline std::optional<std::string> ZSTDDecompress(const std::string_view& from) {
  std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> ctx{ZSTD_createDCtx(), &ZSTD_freeDCtx};
  std::string frame_buffer(ZSTD_DStreamOutSize(), 0);
  std::string decompressed;
  ZSTD_inBuffer in_ref = {.src = from.data(), .size = from.size(), .pos = 0};
  std::size_t decompression_result = 0;
  while (in_ref.pos != in_ref.size) {
    ZSTD_outBuffer out_ref = {.dst = frame_buffer.data(), .size = frame_buffer.size(), .pos = 0};
    decompression_result = ZSTD_decompressStream(ctx.get(), &out_ref, &in_ref);
    if(ZSTD_isError(decompression_result)) {
	  return std::nullopt;
	}
    decompressed.append(frame_buffer.data(), out_ref.pos);
  }
  while (decompression_result) {
    ZSTD_outBuffer out_ref = {.dst = frame_buffer.data(), .size = frame_buffer.size(), .pos = 0};
    decompression_result = ZSTD_decompressStream(ctx.get(), &out_ref, &in_ref);
    if(ZSTD_isError(decompression_result)) {
	  return std::nullopt;
	}
    decompressed.append(frame_buffer.data(), out_ref.pos);
  }
  return decompressed;
}

} // namespace distribuild