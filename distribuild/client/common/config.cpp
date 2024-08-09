#include "client/common/config.h"
#include <stdlib.h>
#include <string>

using namespace std::literals;

namespace distribuild::client::config {

CacheControl GetCacheControl() {
  static const CacheControl result = [] {
    const char* env = getenv("CACHE_CONTROL");
	if (env) {
	  int value = std::stoi(env);
	  return static_cast<CacheControl>(value);
	}
	return CacheControl::Allow;
  }();

  return result;
}

const std::string GetDaemonAddr() {
  static const std::string kDaemonAddr = "127.0.0.1";
  return kDaemonAddr;
}

const std::uint16_t GetDaemonPort() {
  return 8080;
}

} // namespace distribuild::client::config