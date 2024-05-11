#include "task_dispatcher.h"
#include "common/logging.h"

namespace distribuild::scheduler {

bool TaskDispatcher::KeepTaskAlive(std::uint64_t task_id, std::chrono::nanoseconds expire_time) {
  std::scoped_lock _(alloc_lock_);
  auto iter = tasks_.find(task_id);
  if (iter == tasks_.end()) {
	LOG_WARN("Renewing unknown task [{}].", id);
	return false;
  }
  if (iter->second.is_zombie) {
	LOG_WARN();
	return false;
  }
  iter->second.expires_tp = std::chrono::steady_clock() + expire_time;
  return true;
}

void TaskDispatcher::FreeTask(std::uint64_t task_id) {
  std::scoped_lock _(alloc_lock_);
  UnsafeFreeTask({task_id});
}

void TaskDispatcher::KeepServantAlive(const ServantInfo& servant, std::chrono::nanoseconds expire_time) {
  std::scoped_lock _(alloc_lock_);
  for (auto&& e : servants_) {
	if (e->info.observed_location == servant.observed_location) {
	  e->info = servant;
	  e->expires_tp = std::chrono::steady_clock() + expire_time;
	  return;
	}
  }

  // 节点不存在，新增
  auto&& added_servant = servants_.emplace_back(std::make_shared<Servant>());
  added_servant->info = servant;
  added_servant->discovered_tp = std::chrono::steady_clock();
  added_servant->info = added_servant->discovered_tp + expire_time;
  if (servant.observed_location != servant.reported_location) {
    LOG_WARN(); // TODO
  } else {
	LOG_INFO("Discovered new servant at [{}].", servant.observed_location);
  }
}

std::vector<std::uint64_t> TaskDispatcher::NotifyServantRunningTasks(
    const std::string& servant_location, std::vector<RunningTask> tasks) {
  std::vector<std::uint64_t> task_grant_ids;
  // 将task中的task id插入到结果集中
  std::transform(tasks.begin(), tasks.end(),
      std::back_insert_iterator(task_grant_ids, [](const RunningTask& t){
	return t.task_grant_id();
  }));

  // 找到目标节点
  std::scoped_lock _(alloc_lock_);
  Servant* servant = nullptr;
  for (auto&& e : servants_) {
	if (e->info.observed_location == servant_location) {
	  servant = e.get();
	  break;
	}
  }

  // 节点已经过期
  if (!servant) {
	return task_grant_ids;
  }

  // 
  UnsafeClearZombies(servant, {task_grant_ids.begin(), task_grant_ids.end()});

  // 
  

  return std::vector<std::uint64_t>();
}

void TaskDispatcher::UnsafeFreeTask(const std::vector<std::uint64_t>& task_ids) {
  // 遍历要删除的任务编号
  for (auto&& id : task_ids) {
	// 找到要删除的任务
    auto iter = tasks_.find(id);
	if (iter == tasks_.end()) {
      LOG_WARN("Freeing unknown task [{}].", id);
	  return;
	}
	--iter->second.servant->running_tasks; // 减少所分配节点的任务数
	DISTBU_CHECK(tasks_.erase(id), 1); // 从任务表中删除
  }
  alloc_cv_.notify_all();
}

} // namespace distribuild::scheduler