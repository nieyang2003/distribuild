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
#include <Poco/Timer.h>
#include <Poco/Task.h>
#include <Poco/ThreadPool.h>
#include <Poco/TaskManager.h>
#include "daemon/cloud/task.h"
#include "daemon/cloud/temp_file.h"

namespace distribuild::daemon::cloud {

enum class ExecutionStatus {
  Unknown,
  Failed,
  Running,
  NotFound,
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
	pid_t pid;                                            // 进程pid
	std::atomic<bool> is_running = {true};                // 仍在运行？
    std::uint64_t grant_id;                               // scheduler许可
	std::uint32_t ref_count;                              // 等待者人数
	std::chrono::steady_clock::time_point start_tp;       // 任务启动时间点
	std::chrono::steady_clock::time_point completed_tp;   // 任务完成时间点
	std::shared_ptr<Task> task;                           // 原任务引用
	Poco::Event completion_event;                         // 完成事件
	std::string cmd;                                      // 任务运行命令行
	std::optional<TempFile> std_out;
	std::optional<TempFile> std_err;
  };

  /// @brief 用于放入poco线程池的异步任务实现
  class OnExitTask : public Poco::Task {
    Executor* executor_; pid_t pid_; int exit_code_;
   public:
    OnExitTask(Executor* executor, pid_t pid, int exit_code)
	  : Poco::Task("OnExitTask" + std::to_string(uint64_t(executor)))
	  , executor_(executor), pid_(pid), exit_code_(exit_code) {}

    void runTask() override {
	  executor_->OnExitCallback(pid_, exit_code_);
	}
  };

 public:
  /// @brief 单例
  static Executor* Instance();

  /// @brief 构造函数，设置单例的内存限制与任务数
  Executor();

  ~Executor();

  /// @brief 获得最大任务并发数
  size_t GetMaxConcurrency() const;

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
  WaitForTask(std::uint64_t task_id, std::chrono::milliseconds timeout_ms);

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

  void OnTimerClean(Poco::Timer& timer);
  void OnExitCallback(pid_t pid, int exit_code);
  void WaiterProc();

 private:
  std::atomic<bool> exiting_ = {false};           // 是否停止
  std::atomic<std::uint64_t> next_task_id_ = {1}; // 下一次分配的任务的id
  std::atomic<std::size_t> running_tasks_;        // 正在运行的任务数
  std::size_t max_concurrency_;                   // 最大并发数
  std::size_t min_memory_;                        // 接收新任务的最小内存
  std::atomic<std::size_t> task_ran_sum_;         // 执行过的任务总数
  Poco::Timer timer_;                             // 定时器
  Poco::TaskManager task_manager_;                // 任务管理器
  std::unordered_map<std::uint64_t, std::shared_ptr<TaskDesc>> tasks_;
  mutable std::mutex task_mutex_;

  std::thread waitpid_worker_; // 负责等待子进程退出并处理结果的线程
  std::counting_semaphore<1> waitpid_semaphore_{0};
};

}