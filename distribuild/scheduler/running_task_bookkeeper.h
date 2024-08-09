#pragma once

#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>
#include "../build/distribuild/proto/scheduler.grpc.pb.h"

namespace distribuild::scheduler {

/// @brief 记录每个节点地址正在运行的任务
class RunningTaskBookkeeper {
 public:

  /// @brief 更新location正在运行的任务，原有任务直接删除
  /// @param location 
  /// @param tasks 
  void SetRunningTask(const std::string& location, std::vector<RunningTask> tasks);

  /// @brief 直接删除location对应的任务
  /// @param location 
  void DelServant(const std::string& location);

  /// @brief 获取当前所有任务
  /// @return 
  std::vector<RunningTask> GetRunningTasks() const;

 private:
  mutable std::mutex mutex_;

  /// @brief location节点正在运行的任务列表
  std::unordered_map<std::string, std::vector<RunningTask>> running_tasks_;
};

} // namespace distribuild::scheduler