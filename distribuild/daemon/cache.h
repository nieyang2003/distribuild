#pragma once
#include <string>
#include <optional>
#include "common/crypto/blake3.h"
#include "common/crypto/zstd.h"
#include "google/protobuf/any.pb.h"
#include "../build/distribuild/proto/cache.pb.h"

namespace distribuild::daemon {

struct CacheEntry {
  int exit_code;
  std::string std_out;
  std::string std_err;
  google::protobuf::Any extra_info;
  std::string packed;
};

struct CacheHeader {
  uint64_t packed_size;
  uint32_t meta_size;
  uint32_t compression_algorithm;
};

inline std::optional<std::string> TryMakeCacheData(CacheEntry&& entry) {
  // 元数据
  cache::CacheMeta meta;
  meta.set_exit_code(entry.exit_code);
  meta.set_stdout(entry.std_out);
  meta.set_stderr(entry.std_err);
  *meta.mutable_extra_info() = entry.extra_info;
  meta.set_files_check_hash(Blake3(entry.packed));
  auto meta_str = meta.SerializeAsString();
  // header
  CacheHeader header {
	.packed_size = entry.packed.size(),
	.meta_size = (uint32_t)meta_str.size(), // ! metay应该没有这么大吧
  };
  meta_str.clear();
  meta_str.shrink_to_fit();
  // 构造数据
  std::string uncompressed;
  uncompressed.append((char*)&header, sizeof(CacheHeader));
  uncompressed.append(meta.SerializeAsString());
  uncompressed.append(entry.packed);
  // 压缩并返回
  auto result = ZSTDCompress(uncompressed);
  if (!result) {
	return std::nullopt;
  } else {
    return std::move(*result);
  }
}

inline std::optional<CacheEntry> TryParseCacheEntry(std::string&& data) {
  auto decompressed = ZSTDDecompress(data);
  if (!decompressed) {
	return std::nullopt;
  }
  if (decompressed->size() < sizeof(CacheHeader)) {
	return std::nullopt;
  }

  CacheEntry result;
  // header
  CacheHeader header = *(CacheHeader*)decompressed->data();
  if ((header.meta_size + header.packed_size + sizeof(CacheHeader)) != data.size()) {
	return std::nullopt;
  }
  // meta
  auto meta_str = decompressed->substr(sizeof(CacheHeader), header.meta_size);
  cache::CacheMeta meta;
  if (!meta.ParseFromString(meta_str)) {
    return std::nullopt;
  }
  result.exit_code  = std::move(meta.exit_code());
  result.std_out    = std::move(meta.stdout());
  result.std_err    = std::move(meta.stderr());
  result.extra_info = std::move(meta.extra_info());
  // 文件
  result.packed = decompressed->substr(sizeof(CacheHeader) + header.meta_size);
  decompressed->clear();
  if (meta.files_check_hash() != Blake3(result.packed)) {
	return std::nullopt;
  }
  return result;
}

}