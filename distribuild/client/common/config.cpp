#include "./config.h"

#include <stdlib.h>
#include <string>
#include "config.h"

namespace distribuild::client::config {

CacheControl GetCacheControl() {
  static const CacheControl result = [] {
    const char* env = getenv("");
	if (env) {
	  int value = std::stoi(env);
	  return static_cast<CacheControl>(value);
	}
	return CacheControl::Allow;
  }();

  return result;
}

const std::string_view& GetDaemonAddr() {
  return {"127.0.0.1"};
}

const std::uint16_t GetDaemonPort() {
  return 10001;
}

}  // namespace distribuild::client::config