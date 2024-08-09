#pragma once

#include <chrono>
#include <memory>
#include <latch>
#include <atomic>
#include <unordered_map>
#include <grpcpp/grpcpp.h>
#include <Poco/Timer.h>
#include <Poco/Task.h>
#include <Poco/ThreadPool.h>
#include <Poco/TaskManager.h>
#include "../build/distribuild/proto/daemon.grpc.pb.h"
#include "../build/distribuild/proto/daemon.pb.h"
#include "daemon/local/dist_task.h"
#include "daemon/local/config_keeper.h"
#include "daemon/local/task_run_keeper.h"
#include "daemon/local/task_grant_keeper.h"

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

  /// @brief 启动一个任务
  std::uint64_t QueueTask(std::unique_ptr<DistTask> task, std::chrono::steady_clock::time_point start_deadline);

  /// @brief 等待一个任务
  std::pair<std::unique_ptr<DistTask>, WaitStatus> WaitForTask(std::uint64_t task_id, std::chrono::milliseconds timeout);

  void Stop();

  void Join();

 private:
  /// @brief 
  struct TaskDesc : public std::enable_shared_from_this<TaskDesc> {
    enum class State {
	  Pending, Ready, Dispatched, Done
	} state = State::Pending;

	std::mutex mutex;
	Poco::Event completion_event;

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
  class PerformPocoTask : public Poco::Task {
    TaskDispatcher* dispatcher_ = nullptr;
	std::shared_ptr<TaskDesc> task_desc_ = nullptr;
   public:
    PerformPocoTask(TaskDispatcher* dispatcher, std::shared_ptr<TaskDesc> desc)
	  : Poco::Task("PerformPocoTask")
	  , dispatcher_(dispatcher)
	  , task_desc_(desc) {
	}

    virtual void runTask() override {
	  dispatcher_->PerformTask(task_desc_);
	}
  };

  /// @brief 没有编译执行结果，则启动编译任务
  void PerformTask(std::shared_ptr<TaskDesc> task_desc);
  
  /// @brief 尝试从缓存中读取结果
  bool TryReadCache(TaskDesc* task_desc);

  /// @brief 尝试获取执行中的任务知道执行完毕
  bool TryGetExistedResult(TaskDesc* task_desc);

  /// @brief 联系任务所属节点开始执行新的任务
  void StartNewServantTask(TaskDesc* task_desc);

  /// @brief 规定时间内等待节点超时
  void WaitServantTask(cloud::DaemonService::Stub* stub, TaskDesc* task_desc);

  
  std::pair<std::optional<DistTask::DistOutput>, int> WaitServantTask(cloud::DaemonService::Stub* stub, std::uint64_t servant_task_id);

  /// @brief 释放任务
  void FreeServantTask(cloud::DaemonService::Stub* stub, std::uint64_t servant_task_id);

  void OnTimerTimeoutAbort(Poco::Timer& timer);
  void OnTimerKeepAlive(Poco::Timer& timer);
  void OnTimerKilledAbort(Poco::Timer& timer);
  void OnTimerClear(Poco::Timer& timer);

 private:
  Poco::Timer timer_timeout_abort_;
  Poco::Timer timer_keep_alive_;
  Poco::Timer timer_killed_abort_;
  Poco::Timer timer_clear_;
  Poco::TaskManager task_manager_;

  // 任务
  std::mutex tasks_mutex_;
  std::unordered_map<std::uint64_t, std::shared_ptr<TaskDesc>> tasks_;

  // rpc
  std::unique_ptr<scheduler::SchedulerService::Stub> scheduler_stub_; // ！ 或许可以一台控制多台机器

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