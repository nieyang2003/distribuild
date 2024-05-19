#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace distribuild::client {

struct DaemonResponse {
  int status;
  std::string body;
};

using DaemonCallHandler = std::function<DaemonResponse(const std::string&,
	const std::vector<std::string>&,
	const std::vector<std::string_view>&,
	std::chrono::nanoseconds)>;

void SetDaemonCallHandler(DaemonCallHandler handler);

DaemonResponse DaemonCall(const std::string& api,
    const std::vector<std::string>& headers,
	const std::vector<std::string_view>& bodies,
	std::chrono::nanoseconds timeout);

}