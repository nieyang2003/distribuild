#pragma once

namespace distribuild::client {

enum class CacheControl {
  Disallow = 0,
  Allow    = 1,
  Refill   = 2,
};

CacheControl GetCacheControlEnv();

}