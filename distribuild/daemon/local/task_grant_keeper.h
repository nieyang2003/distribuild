#pragma once

#include <chrono>
#include <string>
#include <optional>
#include <memory>
#include <queue>
#include <condition_variable>

#include "scheduler.grpc.pb.h"
#include "scheduler.pb.h"

namespace distribuild::daemon::local {

/// @brief 调用scheduler rpc服务获得许可
class TaskGrantKeeper {
 public:
  struct GrantDesc {
    std::chrono::steady_clock::time_point expire_tp;
	std::uint64_t grant_id;
	std::string servant_location;
  };

  TaskGrantKeeper();

  std::optional<GrantDesc> Get(const EnviromentDesc& desc, const std::chrono::nanoseconds& timeout);

  void Free(std::uint64_t grant_id);

  void Stop();
  void Join();

 private:
  struct EnvGrantKeeper {
	EnviromentDesc env_desc;
    std::uint32_t waiters = 0;
	std::queue<GrantDesc> remaining;
	std::mutex mutex;
	std::condition_variable available_cv;
	std::condition_variable need_more_cv;
  };

  void GrantFetcherProc(EnvGrantKeeper* keeper);

 private:
  std::atomic<bool> leaving_ = false;
  std::mutex mutex_;
  std::unique_ptr<scheduler::SchedulerService::Stub> scheduler_stub_;
  std::unordered_map<std::string, std::unique_ptr<EnvGrantKeeper>> keepers_;
};

} // namespace distribuild::daemon::local