#include "distribuild/common/crypto/zstd.h"

#include "zstd/lib/zstd.h"

namespace distribuild {

std::optional<std::string> ZSTDDecompress(const std::string& data) {
  std::string result;
  auto size = ZSTD_getFrameContentSize(data.data(), data.length());
  if (size == ZSTD_CONTENTSIZE_ERROR) {
	return std::nullopt;
  }
  result.reserve(size + 1024); // TODO
  auto actual_size = ZSTD_decompress(result.data(), size, data.data(), data.length());
  if (ZSTD_isError(actual_size) || actual_size != size) {
    return std::nullopt;
  }
  result.resize(actual_size);
  return result;
}

} // distribuild