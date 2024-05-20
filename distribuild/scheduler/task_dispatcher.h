#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <condition_variable>

#include "running_task_bookkeeper.h"

namespace distribuild::scheduler {

enum class WaitStatus {
  OK = 0,
  EnvNotFound = 1,
  Timeout = 2,
};

// 任务信息
struct TaskInfo {
  EnviromentDesc use_env;  // 只能使用的编译器
};

/// @brief 任务分配情况
struct TaskAlloc {
  std::uint64_t task_id;          // 任务id
  std::string   servant_locaton;  // ip:port
};

/// @brief 节点信息
struct ServantInfo {
  int version;
  std::string observed_location;          // 
  std::string reported_location;          // 
  std::vector<EnviromentDesc> enviroments;// 可用编译器
  std::size_t num_cpu_cores;              // cpu核心数
  std::size_t current_load;               // 当前负载
  std::size_t total_memory;               // 总内存
  std::size_t avail_memory;               // 可用内存
  std::size_t max_tasks;                  // 最大并发线程数
  ServantPriority priority;               // 优先级
};

/// @brief 分配编译服务器
class TaskDispatcher {
 public:
  TaskDispatcher();
  ~TaskDispatcher();

  std::pair<WaitStatus, TaskAlloc> WaitForStartingNewTask(const TaskInfo& task_info, std::chrono::nanoseconds expires_time, 
      std::chrono::steady_clock::time_point timeout, bool prefetching);

  /// @brief 延长任务超时时间
  /// @param task_id 
  /// @param new_expire_time 
  /// @return 
  bool KeepTaskAlive(std::uint64_t task_id, std::chrono::nanoseconds new_expire_time);

  /// @brief 释放任务
  /// @param task_id 
  void FreeTask(std::uint64_t task_id);

  /// @brief 如果在expires_time内没有调用，则宣布死亡
  /// @param servant 
  /// @param expires_time 
  void KeepServantAlive(const ServantInfo& servant, std::chrono::nanoseconds expires_time);

  /// @brief 
  /// @param servant_location 
  /// @param tasks 
  /// @return 
  std::vector<std::uint64_t> NotifyServantRunningTasks(const std::string& servant_location, std::vector<RunningTask> tasks);

  /// @brief 
  /// @return 
  std::vector<RunningTask> GetRunningTasks() const;

 private:
  void UnsafeFreeTask(const std::vector<std::uint64_t>& task_ids);

 private:
  struct Servant {
    ServantInfo info;
	std::chrono::steady_clock::time_point discovered_tp;
	std::chrono::steady_clock::time_point expires_tp;

    std::size_t running_tasks  = 0;
    std::size_t assigned_tasks = 0;
  };

  struct Task {
	TaskInfo info;
    std::uint64_t task_id;
    std::shared_ptr<Servant> servant; // 所分配的节点
	std::chrono::steady_clock::time_point started_tp;
	std::chrono::steady_clock::time_point expires_tp;
	bool is_zombie = false;
  };

 private:
  std::mutex alloc_lock_;
  std::condition_variable alloc_cv_;

  std::uint64_t expir_timer_;
  std::vector<std::shared_ptr<TaskDispatcher::Servant>> servants_;

  std::unordered_map<std::uint64_t, Task> tasks_;
  std::atomic<std::uint64_t> next_task_id_{};

  RunningTaskBookkeeper running_task_bookkeeper_;

  std::size_t min_memory_for_new_task_;
};

} // namespace distribuild::scheduler