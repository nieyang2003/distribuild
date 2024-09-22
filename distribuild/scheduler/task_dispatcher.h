#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <condition_variable>
#include <optional>
#include <Poco/Timer.h>
#include "scheduler/running_task_bookkeeper.h"

namespace distribuild::scheduler {

enum class WaitStatus {
  OK = 0,
  EnvNotFound = 1,
  Timeout = 2,
};

/// @brief 任务分配的id和节点地址
struct TaskAllocation {
  std::uint64_t task_id;          // 任务id
  std::string   servant_location;  // ip:port
};

/// @brief 请求任务信息
struct TaskInfo {
  std::string requester_ip;
  std::uint32_t min_version;
  EnviromentDesc env_desc;
};

/// @brief 节点信息
struct ServantInfo {
  uint32_t version;                       // 版本
  std::string observed_location;          // 观测地址
  std::string reported_location;          // 报告地址
  std::size_t num_cpu_cores;              // cpu核心数
  std::size_t current_load;               // 当前负载数
  std::size_t total_memory_in_bytes;      // 总内存
  std::size_t avail_memory_in_bytes;      // 可用内存
  std::size_t concurrency;                // 最大并发线程数
  ServantPriority priority;               // 优先级
  std::vector<EnviromentDesc> env_decs;   // 可用编译器用于筛选分配节点
};

/// @brief 任务分发者
class TaskDispatcher {
 public:
  static TaskDispatcher* Instance();

  TaskDispatcher();
  ~TaskDispatcher();

  std::pair<std::optional<TaskAllocation>, WaitStatus> WaitForStartingNewTask(const TaskInfo& task_info, std::chrono::milliseconds expires_in,
      std::chrono::steady_clock::time_point timeout, bool prefetching);

  /// @brief 延长任务超时时间
  /// @param task_id 
  /// @param new_expire_time 
  /// @return 
  bool KeepTaskAlive(std::uint64_t task_id, std::chrono::milliseconds new_expire_time);

  /// @brief 释放任务
  /// @param task_id 
  void FreeTask(std::uint64_t task_id);

  /// @brief 设置一个现有节点或新节点的超时时间
  /// @param servant 
  /// @param expires_time 
  void KeepServantAlive(const ServantInfo& servant, std::chrono::milliseconds expires_time);

  /// @brief 
  /// @param servant_location 
  /// @param tasks 
  /// @return 
  std::vector<std::uint64_t> NotifyServantRunningTasks(const std::string& servant_location, std::vector<RunningTask> tasks);

  /// @brief 
  /// @return 
  std::vector<RunningTask> GetRunningTasks() const;

 private:

  // 保存在task中和servants中
  struct Servant : public std::enable_shared_from_this<Servant> {
	using Ptr = std::shared_ptr<Servant>;

    ServantInfo servant_info;
	std::chrono::steady_clock::time_point discovered_tp; // 注册时间点
	std::chrono::steady_clock::time_point expires_tp;    // 超时时间点
    std::size_t running_tasks  = 0;  // 正在运行的任务数
    std::size_t assigned_tasks = 0;  // 被分配过的任务总数
  };

  struct Task {
    std::uint64_t task_id;            // 唯一id
	TaskInfo task_info;               // 任务详细信息
    std::shared_ptr<Servant> servant; // 所分配给的节点
	std::chrono::steady_clock::time_point started_tp; // 分配时间
	std::chrono::steady_clock::time_point expires_tp; // 超时时间
	bool is_prefetch = false;
	bool is_zombie = false;
  };

  /// @brief （无锁）获得拥有task运行环境的节点
  /// @param task_info 
  /// @return 
  std::vector<Servant::Ptr> UnsafeGetServantsHasEnv(const TaskInfo& task_info);

  /// @brief （无锁）获得空闲节点
  /// @param task_info 
  /// @return 
  std::vector<Servant::Ptr> UnsafeGetFreeServants(const std::vector<Servant::Ptr> &eligible_servants);

  /// @brief （无锁）从中挑选一个节点
  /// @param free_servants 
  /// @return 
  Servant::Ptr UnsafePickUpFreeServant(std::vector<Servant::Ptr> &free_servants, const std::string& requestor);

  /// @brief （无锁）从中使用过滤函数挑选一个负载最小的节点
  /// @tparam F 
  /// @param free_servants 
  /// @param filter 过滤函数，返回false的节点不能使用
  /// @return 
  template<class F>
  Servant::Ptr UnsafePickUpServant(std::vector<Servant::Ptr> &free_servants, F&& filter);

  /// @brief 可用任务数
  /// @param servant 
  /// @return 
  size_t AvailableTasks(const Servant::Ptr servant);

  /// @brief （无锁）从所有任务中删去属于servant的running_task_ids的任务
  /// @param servant 
  /// @param running_task_ids 
  void UnsafeClearZombies(const Servant::Ptr servant, const std::unordered_set<std::uint64_t>& running_task_ids);

  /// @brief （无锁）删去任务
  /// @param task_ids 
  void UnsafeFreeTask(const std::vector<std::uint64_t>& task_ids);

  /// @brief 定时器函数，服务过期
  void OnTimerExpiration(Poco::Timer& timer);

 private:
  Poco::Timer timer_;

  std::mutex alloc_mutex_;

  std::condition_variable alloc_cv_;

  /// @brief 当前节点
  std::vector<Servant::Ptr> servants_;

  /// @brief 正在运行的所有任务
  std::unordered_map<std::uint64_t, Task> tasks_;

  /// @brief 任务唯一id
  std::atomic<std::uint64_t> next_task_id_{};
  
  /// @brief 
  RunningTaskBookkeeper running_task_bookkeeper_;

  /// @brief 接受任务的最小内存大小
  std::uint64_t min_memory_for_new_task_;
};

template <class F>
inline TaskDispatcher::Servant::Ptr TaskDispatcher::UnsafePickUpServant(std::vector<Servant::Ptr> &servants, F &&filter) {
  if (servants.empty()) return nullptr;

  Servant::Ptr result = servants[0];
  auto min_utilization = std::numeric_limits<double>::max();

  for (auto&& servant : servants) {
	if (!filter(servant)) {
	  continue;
	}
	double utilization = double(servant->running_tasks / servant->servant_info.concurrency); // 线程利用率
	if (utilization < min_utilization) {
	  min_utilization = utilization;
	  result = servant;
	}
  }

  return result;
}

} // namespace distribuild::scheduler