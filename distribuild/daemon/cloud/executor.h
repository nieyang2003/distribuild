#pragma once

#include <vector>
#include <optional>
#include <memory>
#include <chrono>
#include <mutex>
#include <semaphore>
#include <latch>
#include <unordered_set>
#include <unordered_map>
#include <coroutine>
#include <thread>

#include "distribuild/daemon/cloud/task.h"
#include "distribuild/daemon/cloud/temp_file.h"

namespace distribuild::daemon::cloud {

enum class ExecutionStatus {
  Unknown,
  Failed,
  Running,
  NotFound,
};

enum class UnaccepteReason {
  Unknown,
  Overload,
};

class Executor {
 public:
  struct Input {
    TempFile std_in;
	TempFile std_out;
	TempFile std_err;
  };
  struct Output {
    int exit_code;
	std::string std_out;
	std::string std_err;
  };
  struct RunningTask {
    std::uint64_t servant_task_id;
	std::uint64_t task_grant_id;
	std::shared_ptr<Task> task;
  };
 private:
  /// @brief 执行器在运行的任务的描述
  struct TaskDesc : std::enable_shared_from_this<TaskDesc> {
	pid_t pid;
	std::atomic<bool> running = {true};
    std::uint64_t grant_id;
	std::uint32_t ref_count;
	std::chrono::steady_clock::time_point start_tp;
	std::chrono::steady_clock::time_point completed_tp;
    std::optional<TempFile> std_out;
	std::optional<TempFile> std_err;
	std::shared_ptr<Task> task;
	std::latch compile_latch{1};
	std::string cmd;
  };

 public:
  /// @brief 单例
  static Executor* Instance();

  /// @brief 构造函数
  Executor();

  /// @brief 获得最大任务并发数
  std::pair<std::size_t, UnaccepteReason> GetMaxTasks() const;

  /// @brief 放入进程执行
  /// @param grant_id 
  /// @param task 预处理好的任务
  /// @return 进程pid
  std::optional<std::uint64_t> TryQueueTask(std::uint64_t grant_id, std::shared_ptr<Task> task);

  /// @brief 添加任务引用计数
  bool TryAddTaskRef(std::uint64_t task_id);

  /// @brief 在超时时间内等待任务完成
  /// @param task_id 
  /// @param timeout 
  /// @return 
  std::pair<std::shared_ptr<Task>, ExecutionStatus>
  WaitForTask(std::uint64_t task_id, std::chrono::nanoseconds timeout);

  /// @brief 释放任务
  /// @param task_id 
  void FreeTask(std::uint64_t task_id);

  /// @brief 获取所有正在运行的任务
  /// @return 
  std::vector<RunningTask> GetAllTasks() const;

  /// @brief 杀死所有超时的任务
  /// @param expired_grant_ids 
  void KillExpiredTasks(const std::unordered_set<std::uint64_t>& expired_grant_ids);

  void Stop();
  void Join();

 private:
  std::optional<std::uint64_t> TryStartingNewTaskUnsafe();
  void KillTask(TaskDesc* task);

  void OnTimerClean();
  void OnExitCallback(pid_t pid, int exit_code);
  void WaiterProc();

 private:
  std::atomic<bool> exiting_ = {false};           // 是否停止
  std::atomic<std::uint64_t> next_task_id_ = {1}; // 下一次分配的任务的id
  std::atomic<std::size_t> running_tasks_;        // 正在运行的任务数
  std::size_t max_concurrency_;                   // 最大并发数
  std::size_t min_memory_;                        // 接收新任务的最小内存
  std::atomic<std::size_t> task_ran_;             // 执行过的任务总数
  std::unordered_map<std::uint64_t, std::shared_ptr<TaskDesc>> tasks_;
  mutable std::mutex task_mutex_;
  // 未接受任务的原因
  UnaccepteReason unaccepte_reason_ = UnaccepteReason::Unknown;

  std::thread waitpid_worker_; // 负责等待子进程退出并处理结果的线程
  std::counting_semaphore<1> waitpid_semaphore_{0};
};

}