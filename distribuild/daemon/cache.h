#pragma once

#include <vector>
#include <string>

namespace distribuild::daemon {

struct CacheEntry {
  int exit_code;
  std::string std_out;
  std::string std_err;
  std::vector<std::pair<std::string, std::string>> output_files;
};

}