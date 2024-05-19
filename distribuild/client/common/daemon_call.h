#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace distribuild::client {

struct DaemonResponse {
  int status;
  std::string body;
};

DaemonResponse DaemonCall(const std::string& api,
    const std::vector<std::string>& headers,
	const std::vector<std::string_view>& bodies,
	std::chrono::nanoseconds timeout);

}