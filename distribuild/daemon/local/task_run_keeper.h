#pragma once

#include <string>
#include <optional>
#include <memory>

#include "scheduler.grpc.pb.h"
#include "scheduler.pb.h"

namespace distribuild::daemon::local {

/// @brief 调用scheduler rpc服务同步当前正在运行的任务
class TaskRunKeeper {
 public:
  struct TaskDesc{
    std::string servant_location;
    std::uint64_t servant_task_id;
  };

  TaskRunKeeper();
  ~TaskRunKeeper();
  
  /// @brief 查找运行的任务
  /// @param task_digest 
  /// @return 
  std::optional<TaskDesc> TryFindTask(const std::string& task_digest) const;

  void Stop();
  void Join();
 
 private:
  void OnTimerRefresh();

 private:
  std::unique_ptr<scheduler::SchedulerService::Stub> scheduler_stub_;
  std::mutex mutex_;
  std::unordered_map<std::string, TaskDesc> running_tasks_;
  std::chrono::steady_clock::time_point last_update_time_;
  std::uint64_t sync_timer_;
};

} // namespace distribuild::daemon::local