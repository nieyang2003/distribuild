#pragma once

namespace distribuild::client {

enum class CacheStatus {
  Disallow = 0,
  Allow    = 1,
  Refill   = 2,
};

}