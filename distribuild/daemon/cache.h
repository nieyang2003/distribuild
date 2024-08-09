#pragma once
#include <string>
#include "google/protobuf/any.pb.h"

namespace distribuild::daemon {

struct CacheEntry {
  int exit_code;
  std::string std_out;
  std::string std_err;
  google::protobuf::Any extra_info;
  std::string files;
};

}