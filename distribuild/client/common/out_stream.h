#pragma once
#include <string>
#include <memory>
#include <vector>
#include <span>
#include <zstd.h>
#include <blake3.h>

namespace distribuild::client {

/// @brief 二进制数据流
class OutStream {
public:
    virtual ~OutStream() = default;
    virtual void Write(const char* data, std::size_t bytes) = 0;
};

/// @brief 简单存储数据的数据流
class TransparentOutStream : public OutStream {
public:
    void Write(const char* data, std::size_t bytes) override;
private:
    std::string m_buffer;
};

/// @brief zstd算法压缩源代码的数据流
class ZstdOutStream : public OutStream {
 public:
  ZstdOutStream();
  /// 写入内部成员Chunks
  void Write(const char* data, std::size_t bytes) override;
  /// 获得结果并清空Chunks
  std::string GetResult();
 private:
  /// @brief 刷新内部缓冲区
  void Flush();
 private:
  inline static constexpr std::size_t kChunkSize = 128 * 1024;
  struct Chunk {
    std::unique_ptr<char[]> buffer{new char[kChunkSize]};
	std::size_t used = 0;
  };

  std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> ctx_{ZSTD_createCCtx(), &ZSTD_freeCCtx};
  std::vector<Chunk> chunks_{1}; // 至少一块
};

/// @brief 使用BLAKE3将输入流生成键值
class Blake3OutStream : public OutStream {
 public:
  Blake3OutStream();

  void Write(const char* data, std::size_t bytes) override;

  std::string GetResult() const;

  void Finalize();

 private:
  blake3_hasher state_;
  uint8_t digest_[BLAKE3_OUT_LEN];
};

/// @brief 转发流，调用所有成员的Write方法
class ForwardOutStream : public OutStream {
 public:
  explicit ForwardOutStream(std::span<OutStream*> streams)
    : streams_(streams) {}

  void Write(const char* data, std::size_t bytes) override {
    for (auto&& e : streams_) {
      e->Write(data, bytes);
    }
  }
 private:
  std::span<OutStream*> streams_;
};

} // namespace distribuild