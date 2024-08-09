#pragma once

#include <chrono>
#include <mutex>
#include <unordered_set>
#include <condition_variable>
#include <Poco/Timer.h>

namespace distribuild::daemon::local {

/// @brief 用户监控任务是否超过负载
class TaskMonitor {
 public:
  static TaskMonitor* Instance();

  TaskMonitor();
  ~TaskMonitor();

  /// @brief 
  /// @param pid 
  /// @param lightweight 
  /// @param timeout 
  /// @return 
  bool WaitForNewTask(pid_t pid, bool lightweight, std::chrono::nanoseconds timeout);

  /// @brief 
  /// @param pid 
  void DropTask(pid_t pid);
 
 private:
  // 检查进程是否存活并清理
  void OnTimerCheckAliveProc(Poco::Timer& timer);

 private:
  Poco::Timer timer_;

  /// @brief 最大重量级任务数
  std::size_t max_heavy_tasks_;

  /// @brief 最大轻量级任务数
  std::size_t max_light_tasks_;

  /// @brief 当前重量级别任务数量
  std::atomic<std::size_t> heavy_tasks_;

  /// @brief 当前轻量级别任务数量
  std::atomic<std::size_t> light_tasks_;

  /// @brief 已允许的任务的pid
  std::unordered_set<pid_t> permissions_granted_;
  std::mutex permission_mutex_;

  /// @brief 条件变量，用于阻塞任务许可
  std::condition_variable permission_cv_;
};

} // namespace distribuild::daemon::cloud