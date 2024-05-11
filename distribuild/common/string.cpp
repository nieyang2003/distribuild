#include "string.h"
#include "logging.h"

namespace distribuild {

std::vector<std::string_view> Split(std::string_view s, std::string_view delim, bool keep_empty) {
  std::vector<std::string_view> splited;
  if (s.empty()) {
	return splited;
  }
  DISTBU_CHECK(!delim.empty());

  auto cur = s;
  while (true) {
    auto pos = cur.find(delim);
	if (pos != 0 || keep_empty) {
	  splited.push_back(cur.substr(0, pos));
	}
	if (pos == std::string_view::npos) {
	  break;
	}
	cur = cur.substr(pos + delim.size());
	if (cur.empty()) {
	  if (keep_empty) {
		splited.push_back("");
	  }
	  break;
	}
  }

  return splited;
}

}