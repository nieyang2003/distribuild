#include "io.h"

#include "distribuild/common/logging.h"

#include <fstream>
#include <fcntl.h>
#include <poll.h>
#include <algorithm>

using namespace std::literals;

namespace distribuild::client {

std::ptrdiff_t WriteTo(int fd, const std::string_view& data, std::size_t start) {
  DISTBU_CHECK(start < data.size());
  do {
    int bytes = write(fd, data.data() + start, data.size() - start);
	if (bytes > 0) return bytes;
	if (errno == EINTR) continue;
	if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
	return -1;
  } while (true);
}

std::ptrdiff_t ReadTo(int fd, char* data, std::size_t bytes) {
  while (true) {
	auto bytes_read = read(fd, data, bytes);
	if (bytes_read == -1 && errno == EINTR) {
		continue;
	}
	return bytes_read;
  }
}

void WriteAll(const std::string& filename, const std::string_view& data) {
  std::ofstream ofs(filename, std::ios::binary);
  ofs.write(data.data(), data.size());
  DISTBU_CHECK(ofs, "写入文件 '{}' 失败", filename);
}

void SetNonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  DISTBU_CHECK(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

bool WaitForEvent(int fd, int event, std::chrono::steady_clock::time_point timeout_tp) {
  pollfd fds;
  fds.fd = fd;
  fds.events = event;
  auto result = poll(&fds, 1, std::max(0ns,
      (timeout_tp - std::chrono::steady_clock::now())) / 1ms); // 确保>=0
  DISTBU_CHECK(result > 0);
  return result == 1;
}

}