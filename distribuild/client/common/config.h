#pragma once
#include <string>

namespace distribuild::client {

enum class CacheControl {
  Disallow = 0,
  Allow    = 1,
  Refill   = 2,
};

namespace config {

CacheControl GetCacheControl();

const std::string GetDaemonAddr();

const std::uint16_t GetDaemonPort();

} // namespace config

}