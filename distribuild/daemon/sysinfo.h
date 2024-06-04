#pragma once

#include <chrono>
#include <string>

namespace distribuild::daemon {

std::size_t GetMemoryAvailable();

std::size_t GetDiskAvailableSize(const std::string& dir);

} // namespace distribuild::daemon