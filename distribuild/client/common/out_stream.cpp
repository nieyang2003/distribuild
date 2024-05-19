#include "out_stream.h"

#include "distribuild/common/logging.h"

namespace distribuild::client {

void TransparentOutStream::Write(const char* data, std::size_t bytes) {
    m_buffer.append(data, bytes);
}

ZstdOutStream::ZstdOutStream() {
  ZSTD_initCStream(ctx_.get(), ZSTD_fast);
}

void ZstdOutStream::Write(const char* data, std::size_t bytes) {
  ZSTD_inBuffer in_buf{.src = data, .size = bytes, .pos = 0};

  while (in_buf.pos != in_buf.size) {
	// 分配内存块
	if (chunks_.back().used == kChunkSize) {
	  chunks_.emplace_back(); // 添加新块
	}
    // 计算位置
    auto chunk_bytes_left = kChunkSize - chunks_.back().used;
	ZSTD_outBuffer out_buf = {
		.dst = chunks_.back().buffer.get() + chunks_.back().used,
		.size = (kChunkSize - chunks_.back().used),
		.pos = 0
	};
	//压缩
	auto result = ZSTD_compressStream2(ctx_.get(), &out_buf, &in_buf, ZSTD_e_continue);
	DISTBU_CHECK(!ZSTD_isError(result), "ZSTD压缩失败");
    // 更新块内存
	chunks_.back().used += out_buf.pos;
  }
}

std::string ZstdOutStream::GetResult() {
  Flush();
  std::size_t bytes = 0;
  for (auto&& e : chunks_) {
	bytes += e.used;
  }
  std::string result;
  result.reserve(bytes);
  for (auto&& e : chunks_) {
	result.append(e.buffer.get(), e.used);
  }
  chunks_.clear();
  return result;
}

void ZstdOutStream::Flush() {
  ZSTD_inBuffer in_buf{};

  while (true) {
	// 分配内存块
	if (chunks_.back().used == kChunkSize) {
	  chunks_.emplace_back(); // 添加新块
	}
	// 计算位置
	auto chunk_bytes_left = kChunkSize - chunks_.back().used;
	ZSTD_outBuffer out_buf = {
		.dst = chunks_.back().buffer.get() + chunks_.back().used,
		.size = (kChunkSize - chunks_.back().used),
		.pos = 0
	};
	// 重写压缩
	auto result = ZSTD_compressStream2(ctx_.get(), &out_buf, &in_buf, ZSTD_e_end);
	DISTBU_CHECK(!ZSTD_isError(result), "ZSTD压缩失败");
	// 更新块内存
	chunks_.back().used += out_buf.pos;

	if (result == 0) break; // 退出
  }
}

Blake3OutStream::Blake3OutStream() {
  blake3_hasher_init(&state_);
}

void Blake3OutStream::Write(const char* data, std::size_t bytes) {
  blake3_hasher_update(&state_, data, bytes);
}

std::string Blake3OutStream::GetResult() const {
  std::string result;
  for (int i = 0; i < sizeof(digest_); ++i) {
	char buf[8];
	snprintf(buf, sizeof(buf), "%02x", digest_[i]);
    result += buf;
  }
  return result;
}

void Blake3OutStream::Finalize() {
  blake3_hasher_finalize(&state_, digest_, BLAKE3_OUT_LEN);
}

}  // namespace distribuild::client