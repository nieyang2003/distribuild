#pragma once

#include <optional>
#include <vector>
#include <string>

namespace distribuild::daemon::config {

std::optional<std::size_t> TryGetMemorySize() {
  return 1024 * 1024 * 1024 * 10; /* 10G */
}

std::size_t GetMemoryAvailable();

std::size_t GetMemoryTotal();

// TODO:
std::string GetCompilerDirs();

std::vector<std::string> GetCompilersUsable();

}