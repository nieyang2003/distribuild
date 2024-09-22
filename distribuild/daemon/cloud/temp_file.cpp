#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include "daemon/cloud/temp_file.h"
#include "common/spdlogging.h"
#include "common/io.h"

using namespace std::literals;

namespace distribuild::daemon::cloud {

TempFile::TempFile(const std::string& temp_dir) {
  // 名称
  auto filename = fmt::format("{}/distribuild_{}_XXXXXX", temp_dir,
                    std::chrono::steady_clock::now().time_since_epoch() / 1ns);
  char temp_name[256]{};
  snprintf(temp_name, sizeof(temp_name), "%s", filename.c_str());

  // 创建临时文件
  fd_ = mkostemps(temp_name, 0, O_CLOEXEC);
  DISTBU_CHECK_FORMAT(fd_, "创建临时文件失败");

  // 路径
  char path[256];
  auto size = readlink(("/proc/self/fd/" + std::to_string(fd_)).c_str(), path, sizeof(path));
  DISTBU_CHECK_FORMAT(size > 0, "获取临时文件名失败");

  path_.assign(path, size);
  LOG_DEBUG("创建临时文件：`{}`", path_);
}

TempFile::~TempFile() {
  Close();
}

void TempFile::Write(const std::string& data) {
  LOG_DEBUG("主动写入 data.size() = {}", data.size());
  distribuild::WriteTo(fd_, data, 0);
}

std::string TempFile::ReadAll() const {
  DISTBU_CHECK(lseek(fd_, 0, SEEK_SET) == 0);
  std::ifstream ifs(path_, std::ios::binary | std::ios::ate);
  DISTBU_CHECK_FORMAT(ifs, "文件打开失败");
  std::ifstream::pos_type file_size = ifs.tellg(); // 文件大小
  ifs.seekg(0, std::ios::beg); // 移动到开头
  std::string buffer;
  buffer.resize(file_size); // 重置buffer大小
  ifs.read(buffer.data(), file_size); // 读取
  return buffer;
}

void TempFile::Close() {
  if (fd_) close(fd_);
  fd_ = 0;
  path_.clear();
}

} // namespace distribuild::daemon::cloud