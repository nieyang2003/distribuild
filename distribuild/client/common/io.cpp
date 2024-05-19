#include "io.h"

#include "distribuild/common/logging.h"

#include <fstream>
#include <fcntl.h>

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

}