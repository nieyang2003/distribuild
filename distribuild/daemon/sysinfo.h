#pragma once

#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <sys/sysinfo.h>

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

inline ProcMemInfo GetProcMemInfo() {
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

} // namespace

namespace distribuild::daemon {

/// @brief 获取可用内存
inline std::size_t GetAvailMemory() {
  ProcMemInfo info = GetProcMemInfo();
  return info.mem_available * 1024;
}

/// @brief 获取总内存
inline std::size_t GetTotalMemory() {
  ProcMemInfo info = GetProcMemInfo();
  return info.mem_total * 1024;
}

/// @brief 获取cpu核心数量
/// @return 
inline std::size_t GetNumCPUCores() {
  static size_t kNumCpuCores = get_nprocs();
  return kNumCpuCores;
}

/// @brief 
/// @return 
inline std::size_t GetCurrentLoad(std::chrono::seconds) {
  // 直接获取最近一分钟负载
  double loadavg = 0.0;
  DISTBU_CHECK(getloadavg(&loadavg, 1) == 1);
  return static_cast<std::size_t>(std::ceil(loadavg));
}

/// @brief 解析出字节数（1G、1M、1K）
/// @param size_str_view 
/// @return 
inline std::size_t ParseMemorySize(const std::string_view& size_str_view) {
  std::string size_str(size_str_view);
  std::uint64_t scale = 1;

  if (size_str.back() == 'G') {
    scale = 1 << 30;
    size_str.pop_back();
  } else if (size_str.back() == 'M') {
    scale = 1 << 20;
    size_str.pop_back();
  } else if (size_str.back() == 'K') {
    scale = 1 << 10;
    size_str.pop_back();
  } else if (size_str.back() == 'B') {
    size_str.pop_back();
  }
  // 默认为字节数

  return std::stoul(size_str) * scale;
}

} // namespace distribuild::daemon