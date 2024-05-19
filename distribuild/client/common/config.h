#pragma once

#include <string>

namespace distribuild::client {

enum class CacheControl {
  Disallow = 0,
  Allow    = 1,
  Refill   = 2,
};

// TODO: 实现简单的本地配置加载
namespace config {

CacheControl GetCacheControl();

const std::string_view& GetDaemonAddr();
const std::uint16_t GetDaemonPort();

} // namespace config

}