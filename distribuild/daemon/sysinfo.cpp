#include "sysinfo.h"

#include <fstream>
#include <sstream>

namespace distribuild::daemon {

namespace {

struct ProcMemInfo {
  std::int64_t mem_total = -1;
  std::int64_t mem_free = -1;
  std::int64_t mem_available = -1;  // since 3.14
  std::int64_t buffers = -1;
  std::int64_t cached = -1;
  std::int64_t swap_total = -1;
  std::int64_t swap_free = -1;
};

ProcMemInfo GetProcMemInfo() {
  ProcMemInfo info;
  std::ifstream ifs("/proc/meminfo");

  std::string line_buf;
  while (std::getline(ifs, line_buf)) {
	std::istringstream iss(line_buf);
	std::string  key;
	std::int64_t value;
	iss >> key;
	iss >> value;

	if (key == "MemTotal:") {
      info.mem_total = value;
    } else if (key == "MemFree:") {
      info.mem_free = value;
    } else if (key == "MemAvailable:") {
      info.mem_available = value;
    } else if (key == "Buffers:") {
      info.buffers = value;
    } else if (key == "Cached:") {
      info.cached = value;
    } else if (key == "SwapTotal:") {
      info.swap_total = value;
    } else if (key == "SwapFree:") {
      info.swap_free = value;
    }
  }

  return info;
}

}

std::size_t GetMemoryAvailable() {
  ProcMemInfo info = GetProcMemInfo();
  return info.mem_total * 1024;
}

}