#include "running_task_bookkeeper.h"

namespace distribuild::scheduler {

void RunningTaskBookkeeper::SetServant(const std::string& location, std::vector<RunningTask> tasks) {
  std::scoped_lock _(mutex_);
  running_tasks_.erase(location);
  running_tasks_.emplace(location, std::move(tasks));
}

void RunningTaskBookkeeper::DelServant(const std::string& location) {
  std::scoped_lock _(mutex_);
  running_tasks_.erase(location);
}

std::vector<RunningTask> RunningTaskBookkeeper::GetRunningTasks() const {
  std::vector<RunningTask> result;
  std::scoped_lock _(mutex_);
  for (auto&& [k, v] : running_tasks_) {
    result.insert(result.begin(), v.begin(), v.end());
  }
  return result;
}

}