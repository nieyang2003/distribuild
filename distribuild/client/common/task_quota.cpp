#include "task_quota.h"

#include <fmt/format.h>
#include <thread>

#include "daemon_call.h"
#include "distribuild/common/logging.h"

using namespace std::literals;

namespace distribuild::client {

void ReleaseTaskQuota() {
  std::string body = fmt::format("{{\"requestor_pid\": {}}}", getpid());
  DaemonCall("/local/release_quota", {"Content-Type: application/json"}, {body}, 5s);
}

TaskQuota distribuild::client::TryAcquireTaskQuota(bool lightweight, std::chrono::nanoseconds timeout) {
  // 报文
  std::string api = "/local/acquire_quota";
  std::vector<std::string> headers = {"Content-Type: application/json"};
  std::string body = fmt::format(
	"{{"
	"\"timeout_ms\": {}, "
	"\"lightweight\": {}, "
	"\"requestor_pid\": {}"
	"}}",
	timeout / 1ms, lightweight ? "true" : "false", getpid()
  );
  // http请求
  DaemonResponse response = DaemonCall(api, headers, {body}, timeout + 10s);
  // 处理响应
  if (response.status == 200) { // 已经接受
    // 利用析构构造
	// reinterpret_cast 是一种强制类型转换，用于将一个指针转换为另一种指针类型，或将整数转换为指针类型等。
    return std::shared_ptr<void>(reinterpret_cast<void*>(1), [](auto) { ReleaseTaskQuota(); });
  } else if (response.status == 503) {
    // do nothing
  } else if (response.status == -1) {
    LOG_ERROR("无法访问http服务");
	std::this_thread::sleep_for(1s);
  } else {
    LOG_ERROR("错误的http状态码: ", response.status);
	std::this_thread::sleep_for(1s);
  }

  // 未响应
  return nullptr;
}

TaskQuota AcquireTaskQuota(bool lightweight) {
  auto start = std::chrono::steady_clock::now();

  while (true) {
	auto acquired = TryAcquireTaskQuota(lightweight, 10s);
    if (acquired) {
      return acquired;
    }
    
	int threshold = 30;
	auto waited = std::chrono::steady_clock::now() - start;
	if (threshold && waited / 1s > threshold) {
      LOG_WARN("无法获得许可，服务器可能过载，已等待 {} 秒", waited / 1s);
    }
  }
}

} // namespace distribuild::client