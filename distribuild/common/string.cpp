#include "string.h"
#include "logging.h"

namespace distribuild {

bool StartWith(const std::string_view& s, const std::string_view& pattern) {
  return s.size() >= pattern.size() && s.substr(0, pattern.size()) == pattern;
}

bool EndWith(const std::string_view& s, const std::string_view& pattern) {
  return s.size() >= pattern.size() && s.substr(s.size() - pattern.size()) == pattern;
}

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