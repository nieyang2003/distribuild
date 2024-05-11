#pragma once

#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>

namespace distribuild::scheduler {

/// @brief 记录正在运行的任务
class RunningTaskBookkeeper {
 public:
  void SetServant(const std::string& location, std::vector<RunningTask> tasks);
  void DelServant(const std::string& location);
  std::vector<RunningTask> GetRunningTasks() const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::vector<RunningTask>> running_tasks_;
};

} // namespace distribuild::scheduler