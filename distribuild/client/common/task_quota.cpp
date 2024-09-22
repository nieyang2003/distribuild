#include "client/common/task_quota.h"
#include "client/common/daemon_call.h"
#include "client/common/config.h"
#include "common/spdlogging.h"
#include <thread>
#include <sstream>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/Exception.h>

using namespace std::literals;

namespace distribuild::client {

void ReleaseTaskQuota() {
  Json::Value req_body;
  req_body["requestor_pid"] = getpid();
  auto&& [response, body] = DaemonHttpCall("/local/release_quota", req_body, 5);
  if (response && response->getStatus() != 200) {
	LOG_ERROR("失败：status: {} {} {}", (int)response->getStatus(), response->getReason(), body);
  }
}

TaskQuota TryAcquireTaskQuota(bool lightweight, std::chrono::seconds timeout) {
  Json::Value req_body;

  req_body["ms_to_wait"] = timeout / 1s;
  req_body["lightweight"] = lightweight;
  req_body["requestor_pid"] = getpid();

  auto&& [response, body] = DaemonHttpCall("/local/acquire_quota", req_body, 5);
  if (response && response->getStatus() == 200) {
	return std::shared_ptr<void>(reinterpret_cast<void*>(1), [](auto) { ReleaseTaskQuota(); });
  }
  if (response) {
    LOG_ERROR("失败：status: {} {}\n{}", (int)response->getStatus(), response->getReason(), body);
  }
  std::this_thread::sleep_for(1s);
  return nullptr;
}

TaskQuota AcquireTaskQuota(bool lightweight) {
  auto start = std::chrono::steady_clock::now();

  while (true) {
	auto acquired = TryAcquireTaskQuota(lightweight, 10s);
    if (acquired) {
      return acquired;
    }

	int threshold = 30; // ! config
	auto waited = std::chrono::steady_clock::now() - start;
	if (threshold && waited / 1s > threshold) {
      LOG_WARN("无法获得许可，服务器可能过载，已等待 {} 秒", waited / 1s);
    }
  }
}

} // namespace distribuild::client