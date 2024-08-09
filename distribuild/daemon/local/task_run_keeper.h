#pragma once

#include <string>
#include <optional>
#include <memory>
#include <Poco/Timer.h>
#include "../build/distribuild/proto/scheduler.grpc.pb.h"
#include "../build/distribuild/proto/scheduler.pb.h"

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
  std::optional<TaskDesc> TryFindTask(const std::string& task_digest) const;

  void Stop();
  void Join();
 
 private:
  void OnTimerRefresh(Poco::Timer& timer);

 private:
  Poco::Timer timer_;
  std::unique_ptr<scheduler::SchedulerService::Stub> scheduler_stub_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, TaskDesc> running_tasks_;
  std::chrono::steady_clock::time_point last_update_time_; // 记录上次更新时间，当断开与scheduler连接后用来判断清除数据
  std::uint64_t sync_timer_;
};

} // namespace distribuild::daemon::local