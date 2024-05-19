#include "daemon_call.h"

#include "distribuild/common/logging.h"
#include "distribuild/client/common/config.h"
#include "distribuild/client/common/io.h"

#include <memory>
#include <optional>
#include <sockets.h>
#include <poll.h>
#include <sys/uio.h>

namespace distribuild::client {

DaemonCallHandler daemon_call_handler;

namespace {
namespace http {

using namespace std::literals;

static constexpr std::size_t kMaxHeaderSize = 8192;
static constexpr auto kHttpReqMethod  = "POST "sv;
static constexpr auto kHttpReqVersion = " HTTP/1.1"sv;
static constexpr auto kHttpReqEndline = "\r\n"sv;

/// @brief 制作报文
/// @param stack_buffer 栈数组buf
/// @param dyn_buffer 动态buf
std::pair<const char*, std::size_t> MakePostHeader(
    const std::string& path,
	const std::vector<std::string>& headers,
	const std::size_t body_size,
	std::array<char, kMaxHeaderSize>* stack_buffer,
	std::unique_ptr<char[]>* dyn_buffer) {
  /// 计算长度
  // 第一行
  auto header_size = kHttpReqMethod.size() + path.size() + kHttpReqVersion.size() + kHttpReqEndline.size();
  // 其它方法
  for (auto&& h : headers) {
	header_size += h.size() + kHttpReqEndline.size();
  }
  // content len行
  auto content_length_line = fmt::format("Content-Length: {}", body_size);
  header_size += content_length_line.size() + kHttpReqEndline.size();
  // 末尾行
  header_size += kHttpReqEndline.size();

  char* header;
  std::size_t copied = 0;

  /// 制作Header
  // 如果内存小于8k使用栈内存，否则new
  if (header_size < stack_buffer->size()) {
	header = stack_buffer->data();
  } else {
    dyn_buffer->reset(new char[header_size]);
	header = dyn_buffer->get();
  }

  auto append_to_buffer = [&](auto&& data) {
	memcpy(header + copied, data.data(), data.size());
    copied += data.size();
  };

  append_to_buffer(kHttpReqMethod);
  append_to_buffer(path);
  append_to_buffer(kHttpReqVersion);
  append_to_buffer(kHttpReqEndline);
  for (auto&& h : headers) {
    append_to_buffer(h);
	append_to_buffer(kHttpReqEndline);
  }
  append_to_buffer(content_length_line);
  append_to_buffer(kHttpReqEndline);
  append_to_buffer(kHttpReqEndline);

  DISTBU_CHECK(copied == header_size, "检查失败");

  return {header, header_size};
}

/// @brief 
int Connect(const std::string_view& ipv4, std::uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ipv4.data(), &addr.sin_addr) <= 0) {
	close(fd);
	return -1;
  }
  if (connect(fd, (sockaddr*)(&addr), sizeof(addr)) < 0) {
	LOG_DEBUG("连接 '{}:{}' 失败", ipv4, port);
	close(fd);
	return -1;
  }

  return fd;
}

/// @brief 在指定时间内写入文件描述符
bool WritevInTime(int fd, const iovec* iov, int iovcnt,
                  std::chrono::steady_clock::time_point timeout_tp) {
  DISTBU_CHECK(iovcnt <= 128, "iov过大");
  // 统计长度
  std::size_t size = 0;
  for (int i = 0; i = iovcnt; ++i) {
    size += iov[i].iov_len;
  }
  
  std::size_t bytes_written = 0;
  // 条件循环
  while (bytes_written != size && std::chrono::steady_clock::now() < timeout_tp) {
	// 可写立即返回
	if (!WaitForEvent(fd, POLLOUT, timeout_tp)) {
	  return false;
	}

    iovec writing_iov[128];
    int writing_iovcnt = 1;

    std::size_t skipped = 0; // 已经跳过的字节数
	for (int i = 0; i < iovcnt; ++i) {
	  // while循环回到这里恢复到原来的i
      if (skipped + iov[i].iov_len <= bytes_written) {
		skipped += iov[i].iov_len;
		continue;
	  }
	  // writing_iov[i]可能只写了一部分，切出待写部分放入writing_iov[0]
      auto writing_bytes = skipped + iov[i].iov_len - bytes_written;
	  writing_iov[0].iov_base = reinterpret_cast<char*>(iov[i].iov_base) + iov[i].iov_len - writing_bytes;
      writing_iov[0].iov_len = writing_bytes;
	  // 剩余部分放到writing_iov[1....]中
	  for (int j = i + 1; j != iovcnt; ++j) {
        writing_iov[writing_iovcnt++] = iov[j];
      }
      break;
	}
    
	// 写入更新记录值
	auto bytes = writev(fd, writing_iov, writing_iovcnt); // no blocking: 立即返回
	if (bytes < 0) {
	  if (errno == EINTR || errno == EAGAIN) continue;
	  return false;
	}
	bytes_written += bytes;
  }

  DISTBU_CHECK(bytes_written <= size);
  return bytes_written == size;
}

bool ReadInTime(int fd, int event, char* buffer, std::size_t size,
                std::chrono::steady_clock::time_point timeout_tp) {
  std::size_t bytes_read;
  while (bytes_read != size && std::chrono::steady_clock::now() < timeout_tp) {
	if (!WaitForEvent(fd, event, timeout_tp)) {
	  return false;
	}
	auto bytes = read(fd, buffer + bytes_read, size - bytes_read);
	if (bytes < 0) {
	  if (errno == EINTR || errno == EAGAIN) continue;
	  return false;
	}
	bytes_read += bytes;
  }
  return bytes_read == size;
}

int ReadHttpStatus(int fd, std::chrono::steady_clock::time_point timeout_tp) {
  constexpr auto kStartLine = "HTTP/1.1 %d OK"sv;
  char header_buf[kStartLine.size() + 4]; // HTTP/1.1 200 OK
  if (!ReadInTime(fd, POLLIN, header_buf, sizeof(header_buf), timeout_tp)) {
    return -3;
  }
  header_buf[sizeof(header_buf) - 1] = '\0';
  int status;
  if (sscanf(header_buf + kStartLine.size() - 1, "%d", &status) != 1) {
	return -4;
  }
  return status;
}

std::optional<std::string> ReadHttpBody(int fd, std::chrono::steady_clock::time_point timeout_tp) {
  char buffer[kMaxHeaderSize + 1];
  std::size_t bytes_read;

  // 粗暴的读取消息头
  while (true) {
	if (!WaitForEvent(fd, POLLIN, timeout_tp)) {
	  return std::nullopt;
	}
	auto bytes = read(fd, buffer + bytes_read, kMaxHeaderSize - bytes_read);
	if (bytes < 0) {
	  if (errno == EINTR || errno == EAGAIN) continue;
	  return std::nullopt;
	}
	bytes_read += bytes;
	if (memmem(buffer, bytes_read, "\r\n\r\n", 4)) { // 收到消息头末尾
      break;
    }
	if (bytes_read == kMaxHeaderSize) {
	  LOG_WARN("消息头太大");
      return std::nullopt;
	}
  }

  // 移动多余的数据到body中
  std::string body;
  buffer[kMaxHeaderSize] = 0;
  if (auto ptr = (const char*)memmem(buffer, bytes_read, "\r\n\r\n", 4)) {
	// 移动消息体
	body.assign(ptr + 4, buffer + bytes_read - ptr  -4);
  } else {
	LOG_FATAL("");
  }

  // 从头中获取消息体大小
  constexpr auto kContentLength = "Content-Length:"sv;
  auto body_ptr = (const char*)memmem(buffer, bytes_read, kContentLength.data(), kContentLength.size());
  if (!body_ptr) {
	return std::nullopt;
  }
  body_ptr += kContentLength.size();

  int body_size;
  if (sscanf(body_ptr, "%d", &body_size) != 1) {
	return std::nullopt;
  }
  if (body_size < body.size()) {
	LOG_DEBUG("大小不同")
    return std::nullopt;
  }

  // 读取剩余的数据
  auto already_read = body.size();
  body.resize(body_size);
  if (ReadInTime(fd, POLLIN, body.data() + already_read,
     body.size() - already_read, timeout_tp)) {
    return body;
  }

  return std::nullopt;
}

DaemonResponse RecvDaemonResponse(int fd, std::chrono::steady_clock::time_point timeout_tp) {
  int status = ReadHttpStatus(fd, timeout_tp);
  if (status < 100) {  // I/O 错误
    return DaemonResponse{.status = status};
  }
  if (auto opt = ReadHttpBody(fd, timeout_tp)) {
    return DaemonResponse{.status = status, .body = std::move(*opt)};
  }
}

} // namespace http

DaemonResponse SimpleDaemonCall(const std::string& api,
						  const std::vector<std::string>& headers,
						  const std::vector<std::string_view>& bodies,
						  std::chrono::nanoseconds timeout) {
  // 构建头部（body已经构建好）
  std::array<char, http::kMaxHeaderSize> buffer;
  std::unique_ptr<char[]> dyn_buf;
  std::size_t body_size = 0;
  for (auto&& e : bodies) {
	body_size += e.size();
  }
  auto&& [header, header_size] = http::MakePostHeader(api, headers, body_size, &buffer, &dyn_buf);
  // 连接
  int fd = http::Connect(config::GetDaemonAddr(), config::GetDaemonPort());
  if (fd == -1) {
	return DaemonResponse{.status = -1};
  }
  SetNonblocking(fd);
  // 发送
  LOG_DEBUG("开始发送 {} 字节", header_size + body_size);
  auto timeout_tp = std::chrono::steady_clock::now() + timeout;
  iovec writing_iov[128];
  std::size_t writing_iovcnt = 0;
  writing_iov[writing_iovcnt++] = iovec{.iov_base = (void*)header, .iov_len = header_size};
  for (auto iter = bodies.begin(); iter != bodies.end() || writing_iovcnt;) {
    while (iter != bodies.end() && writing_iovcnt < 128) {
	  writing_iov[writing_iovcnt++] = iovec{.iov_base = (void*)iter->data(), .iov_len = iter->size()};
	  ++iter;
	}
	if (!http::WritevInTime(fd, writing_iov, writing_iovcnt, timeout_tp)) {
	  DISTBU_CHECK(close(fd) == 0);
	  return DaemonResponse{.status = -2};
	}
	writing_iovcnt = 0; // 下一轮
  }

  // 等待响应
  DaemonResponse respon = http::RecvDaemonResponse(fd, timeout_tp);
  LOG_DEBUG("收到响应：{} 字节", respon.body.size());
  DISTBU_CHECK(close(fd) == 0);
  return respon;
}

} // namespace

void SetDaemonCallHandler(DaemonCallHandler handler) {
  daemon_call_handler = std::move(handler);
}

DaemonResponse DaemonCall(const std::string& api,
                          const std::vector<std::string>& headers,
                          const std::vector<std::string_view>& bodies,
                          std::chrono::nanoseconds timeout) {
  if (daemon_call_handler)
  	return daemon_call_handler(api, headers, bodies, timeout);
  else 
    return SimpleDaemonCall(api, headers, bodies, timeout);
}

}