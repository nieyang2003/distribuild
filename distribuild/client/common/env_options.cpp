#include "env_options.h"

#include <stdlib.h>
#include <string>

namespace distribuild::client {

CacheControl GetCacheControlEnv() {
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

}