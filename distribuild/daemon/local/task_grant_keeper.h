#pragma once

#include <chrono>
#include <string>
#include <optional>
#include <memory>
#include <queue>
#include <condition_variable>
#include <Poco/Task.h>
#include <Poco/ThreadPool.h>
#include <Poco/TaskManager.h>
#include "../build/distribuild/proto/scheduler.grpc.pb.h"
#include "../build/distribuild/proto/scheduler.pb.h"

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
	std::condition_variable available_cv; // 授权成功
	std::condition_variable need_more_cv; // 通知申请授权
	Poco::Task* task;
  };

  class GrantFetcherPocoTask : public Poco::Task {
    TaskGrantKeeper* keeper_;
	EnvGrantKeeper* env_;
   public:
    GrantFetcherPocoTask(TaskGrantKeeper* keeper, EnvGrantKeeper* env) 
	  : Poco::Task(std::to_string((uint64_t)env))
	  , keeper_(keeper) , env_(env) {}

	void runTask() override {
      keeper_->GrantFetcherProc(env_);
	}
  };

  void GrantFetcherProc(EnvGrantKeeper* keeper);

 private:
  std::atomic<bool> leaving_ = false;
  Poco::TaskManager task_manager_;
  std::mutex mutex_;
  std::unique_ptr<scheduler::SchedulerService::Stub> scheduler_stub_ = nullptr;
  std::unordered_map<std::string, std::unique_ptr<EnvGrantKeeper>> keepers_;
};

} // namespace distribuild::daemon::local