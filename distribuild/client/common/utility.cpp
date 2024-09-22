#include "client/common/utility.h"
#include "common/spdlogging.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>

namespace distribuild::client {

const std::string& GetSelfExecutable() {
  static std::string self = GetRealPath("/proc/self/exe");
  return self;
}

std::string GetBaseName(const std::string& name) {
  if (auto pos = name.find_last_of('/'); pos != std::string::npos) {
	return name.substr(pos + 1);
  }
  return name;
}

std::string GetRealPath(const std::string& name) {
  char buf[PATH_MAX + 1]{};
  if (realpath(name.c_str(), buf)) {
	return buf;
  }
  return std::string();
}

std::string FindExecutableInPath(
    const std::string& executable,
    const std::function<bool(const std::string&)>& filter) {
  char* path = getenv("PATH");
  
  while (true) {
	char* chr = strchr(path, ':');

	int length{};
	if (!chr) {
		length = strlen(path); // 末尾
	} else {
		length = chr - path; // 有效长度
	}

	auto dir = std::string_view(path, length);
	LOG_DEBUG("在目录 '{}' 中寻找 '{}'", dir, executable);

	auto file = fmt::format("{}/{}", dir, executable);
	auto real_path = GetRealPath(file);
	struct stat buf;
	if (lstat(file.c_str(), &buf) == 0 && real_path != GetSelfExecutable() && filter(real_path)) {
      LOG_TRACE("Found '{}' in '{}'", executable, dir);
	  return file;
	}
	
	if (chr) {
	  path = chr + 1;
	} else {
	  break; // 查找完了所有路径
	}
  }
  LOG_FATAL("Failed to find executable file '{}'", executable);
}

std::pair<std::uint64_t, std::uint64_t> GetFileModifytimeAndSize(const std::string& file) {
  struct stat result;
  DISTBU_CHECK(lstat(file.c_str(), &result) == 0);
  return std::pair(result.st_mtime, result.st_size);
}

}