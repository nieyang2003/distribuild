#pragma once

#include <chrono>
#include <memory>
#include <latch>
#include <atomic>
#include <unordered_map>
#include <grpcpp/grpcpp.h>

#include "daemon.grpc.pb.h"
#include "daemon.pb.h"

#include "distribuild/daemon/local/dist_task.h"
#include "distribuild/daemon/local/config_keeper.h"
#include "distribuild/daemon/local/task_run_keeper.h"
#include "distribuild/daemon/local/task_grant_keeper.h"

namespace distribuild::daemon::local {

/// @brief 接受来自client的http请求，并将任务提交到cloud执行
class TaskDispatcher {
 public:
  static TaskDispatcher* Instance();

  enum class WaitStatus {
    OK, Timeout, NotFound,
  };

  TaskDispatcher();
  ~TaskDispatcher();

  std::uint64_t QueueTask(std::unique_ptr<DistTask> task, std::chrono::steady_clock::time_point start_deadline);

  std::pair<std::unique_ptr<DistTask>, WaitStatus> WaitForTask(std::uint64_t task_id, std::chrono::nanoseconds timeout);

  void Stop();
  void Join();

 private:
  struct TaskDesc : public std::enable_shared_from_this<TaskDesc> {
    enum class State {
	  Pending, Ready, Dispatched, Done
	} state = State::Pending;

	std::mutex mutex;
	std::latch completed_latch{1};

    std::uint64_t task_id;
	std::unique_ptr<DistTask> task;
	std::chrono::steady_clock::time_point start_deadline;
    std::atomic<bool> aborted{false};
	DistTask::DistOutput output;

	std::chrono::steady_clock::time_point start_tp      {};
	std::chrono::steady_clock::time_point ready_tp      {};
	std::chrono::steady_clock::time_point dispatched_tp {};
	std::chrono::steady_clock::time_point completed_tp  {};
    
    std::uint64_t task_grant_id    = 0;
	std::string   servant_location    ;
	std::uint64_t servant_task_id  = 0;
	std::chrono::steady_clock::time_point last_keep_alive_tp;
  };

 private:
  void PerformTask(std::shared_ptr<TaskDesc> task_desc);
  bool TryReadCache(TaskDesc* task_desc);
  bool TryGetExistedResult(TaskDesc* task_desc);
  void StartNewServantTask(TaskDesc* task_desc);

  void WaitServantTask(cloud::DaemonService::Stub* stub, TaskDesc* task_desc);
  std::pair<std::optional<DistTask::DistOutput>, int> WaitServantTask(cloud::DaemonService::Stub* stub, std::uint64_t servant_task_id);
  void FreeServantTask(cloud::DaemonService::Stub* stub, std::uint64_t servant_task_id);

  void OnTimerTimeoutAbort();
  void OnTimerKeepAlive();
  void OnTimerKilledAbort();
  void OnTimerClear();

 private:
  // 任务
  std::mutex tasks_mutex_;
  std::unordered_map<std::uint64_t, std::shared_ptr<TaskDesc>> tasks_;

  // rpc
  std::unique_ptr<scheduler::SchedulerService::Stub> scheduler_stub_; // TODO: 或许可以一台控制多台机器

  // 节点通信
  ConfigKeeper    config_keeper_;
  TaskRunKeeper   task_run_keeper_;
  TaskGrantKeeper task_grant_keeper_;
  
  // 统计
  std::atomic<std::uint64_t> hit_cache_   {0};
  std::atomic<std::uint64_t> existed_times{0};
  std::atomic<std::uint64_t> run_times_   {0};
};

}  // namespace distribuild::daemon::local