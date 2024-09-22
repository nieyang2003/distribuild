#include "scheduler/task_dispatcher.h"
#include "common/spdlogging.h"
#include "common/tools.h"
#include <gflags/gflags.h>

using namespace std::literals;

DEFINE_uint64(min_memory_for_new_task, (1UL << 30) * 2, "接受编译任务的最小内存大小，默认2G");

namespace distribuild::scheduler {

TaskDispatcher* TaskDispatcher::Instance() {
  static TaskDispatcher instance;
  return &instance;
}

TaskDispatcher::TaskDispatcher()
  : timer_(0, 1'000)
  , min_memory_for_new_task_(FLAGS_min_memory_for_new_task) {
  LOG_DEBUG("启动定时器 OnTimerExpiration");
  timer_.start(Poco::TimerCallback<TaskDispatcher>(*this, &TaskDispatcher::OnTimerExpiration));
}

TaskDispatcher::~TaskDispatcher() {
  timer_.stop();
}

std::pair<std::optional<TaskAllocation>, WaitStatus>
TaskDispatcher::WaitForStartingNewTask(
    const TaskInfo& task_info, std::chrono::milliseconds expires_in,
    std::chrono::steady_clock::time_point timeout, bool prefetching) {
  std::unique_lock lock(alloc_mutex_);

  std::vector<Servant::Ptr> free_servants;
  while (true) {
	// 找到有对应编译器的节点
	auto servants_has_env = UnsafeGetServantsHasEnv(task_info);
	if (servants_has_env.empty()) {
	  return {std::nullopt, WaitStatus::EnvNotFound};
	}
	// 找到空闲的机器
	free_servants = UnsafeGetFreeServants(servants_has_env);
	if (!free_servants.empty()) {
      break; // 找到退出
	}
    // 无可用机器，等待空闲机器
	if (alloc_cv_.wait_until(lock, timeout) == std::cv_status::timeout) {
	  LOG_INFO("暂无可用机器");
	  return {std::nullopt, WaitStatus::Timeout};
	}
  }

  // 挑选一个可用的节点
  auto picked = UnsafePickUpFreeServant(free_servants, task_info.requester_ip);
  ++picked->running_tasks;
  ++picked->assigned_tasks;

  // 创建新任务
  auto task_id = next_task_id_.fetch_add(1, std::memory_order_relaxed);
  DISTBU_CHECK(tasks_.count(task_id) == 0);

  auto&& new_task = tasks_[task_id];
  new_task.task_id = task_id;
  new_task.task_info = task_info;
  new_task.servant = std::shared_ptr<Servant>(picked);
  new_task.started_tp = std::chrono::steady_clock::now();
  new_task.expires_tp = new_task.started_tp + expires_in;
  new_task.is_prefetch = prefetching;

  return {{TaskAllocation{.task_id = task_id, .servant_location = picked->servant_info.observed_location}}, WaitStatus::OK};
}

std::vector<TaskDispatcher::Servant::Ptr> TaskDispatcher::UnsafeGetServantsHasEnv(const TaskInfo& task_info) {
  std::vector<Servant::Ptr> eligible_servants;

  // 寻找可用节点
  for (auto&& servant : servants_) {
	auto find_env = [servant, task_info] () {
	  for (auto&& env : servant->servant_info.env_decs) {
		if (env.compiler_digest() == task_info.env_desc.compiler_digest()) {
		  return true;
		}
	  }
	  return false;
	};

	if (!find_env()) {
	  // 节点无对应编译器
	  continue;
	}
	if (servant->servant_info.concurrency <= 0) {
	  // 节点不接受任务
	  continue;
	}
	if (servant->servant_info.version > task_info.min_version) {
	  // 小于节点版本
	  continue;
	}

	eligible_servants.push_back(servant);
  }

  if (eligible_servants.empty()) {
	LOG_WARN("来自 '{}' 没有对应的 '{}' 编译器环境，或者没有可用节点", task_info.requester_ip, task_info.env_desc.compiler_digest());
  }

  return eligible_servants;
}

std::vector<TaskDispatcher::Servant::Ptr> TaskDispatcher::UnsafeGetFreeServants(const std::vector<Servant::Ptr> &eligible_servants) {
  std::vector<Servant::Ptr> free_servants;
  for (auto&& servant : eligible_servants) {
	if (servant->running_tasks >= AvailableTasks(servant)) {
	  LOG_DEBUG("servant->running_tasks = {}, AvailableTasks(servant) = {}", servant->running_tasks, AvailableTasks(servant));
	  // 忙
	  continue;
	}
	free_servants.push_back(servant);
  }
  return free_servants;
}

TaskDispatcher::Servant::Ptr TaskDispatcher::UnsafePickUpFreeServant(std::vector<Servant::Ptr> &free_servants, const std::string &requestor_ip) {
  Servant::Ptr self = nullptr;

  // 查找是否有ip相同的节点
  auto iter = std::find_if(free_servants.begin(), free_servants.end(), [&](auto&& servant){
    return servant->servant_info.observed_location.size() > requestor_ip.size() &&
	       servant->servant_info.observed_location[requestor_ip.size()] == ':' &&
		   StartWith(servant->servant_info.observed_location, requestor_ip);
  });
  if (iter != free_servants.end()) {
	self = *iter;
	free_servants.erase(iter);
  }

  // 寻找是否有专用编译节点，优先使用
  if (auto ptr = UnsafePickUpServant(free_servants, [](Servant::Ptr servant){ 
	    return servant->servant_info.priority == ServantPriority::SERVANT_PRIORITY_DEDICATED &&  
			   servant->servant_info.concurrency > 2 * servant->running_tasks; })) {
    return ptr;
  }
  
  // 是否有其它可用节点
  if (auto ptr = UnsafePickUpServant(free_servants, [](Servant::Ptr servant){ return true; })) {
    return ptr;
  }

  // 没办法，只能使用自己
  LOG_DEBUG("使用了自己");
  DISTBU_CHECK(self);
  DISTBU_CHECK(AvailableTasks(self) != 0);
  return self;
}

size_t TaskDispatcher::AvailableTasks(const Servant::Ptr servant) {
  if (servant->servant_info.total_memory_in_bytes && servant->servant_info.avail_memory_in_bytes < min_memory_for_new_task_) {
  	// 无法接受更多任务
	LOG_DEBUG("avail {}, min {}", servant->servant_info.avail_memory_in_bytes, min_memory_for_new_task_);
  	return servant->running_tasks;
  } else {
	// 平均负载与瞬时负载取大的
    auto load = std::max<size_t>(servant->servant_info.current_load - servant->running_tasks, 0);
	size_t avail_tasks = std::max<size_t>(servant->servant_info.num_cpu_cores - load, 0);
	return std::min(servant->servant_info.concurrency, avail_tasks);
  }
}

void TaskDispatcher::UnsafeClearZombies(const Servant::Ptr servant, const std::unordered_set<std::uint64_t> &running_task_ids) {
  std::size_t non_prefetch_zombies = 0;
  std::vector<std::uint64_t> clearing_task_ids;

  for (auto&& iter = tasks_.begin(); iter != tasks_.end(); ++iter) {
	if (iter->second.servant == servant && 
	    iter->second.is_zombie &&
		running_task_ids.count(iter->second.task_id) == 0) {
      clearing_task_ids.push_back(iter->first);
	  non_prefetch_zombies += !iter->second.is_prefetch;
	}
  }

  UnsafeFreeTask(clearing_task_ids);
}

bool TaskDispatcher::KeepTaskAlive(std::uint64_t task_id, std::chrono::milliseconds expire_time) {
  std::scoped_lock _(alloc_mutex_);
  auto iter = tasks_.find(task_id);
  if (iter == tasks_.end()) {
	LOG_WARN("正在更新未知任务 '{}'.", task_id);
	return false;
  }
  if (iter->second.is_zombie) {
	auto now = std::chrono::steady_clock::now();
	LOG_WARN("尝试keep alive僵尸进程 '{}'，此进程开始于 '{}'，在'{}'时已经过期", task_id,
	  (now - iter->second.started_tp) / 1s, (now - iter->second.expires_tp) / 1s);
	return false;
  }
  iter->second.expires_tp = std::chrono::steady_clock::now() + expire_time;
  return true;
}

void TaskDispatcher::FreeTask(std::uint64_t task_id) {
  std::scoped_lock _(alloc_mutex_);
  UnsafeFreeTask({task_id});
}

void TaskDispatcher::KeepServantAlive(const ServantInfo& servant_info, std::chrono::milliseconds expire_time) {
  std::scoped_lock _(alloc_mutex_);
  // 节点是否存在
  for (auto&& servant : servants_) {
	if (servant->servant_info.observed_location == servant_info.observed_location) {
	  // 找到存在的节点，进行更新
	  servant->servant_info = servant_info;
	  servant->expires_tp = std::chrono::steady_clock::now() + expire_time;
	  return;
	}
  }

  // 节点不存在，新增
  auto&& new_servant = servants_.emplace_back(std::make_shared<Servant>());
  new_servant->servant_info = servant_info;
  new_servant->discovered_tp = std::chrono::steady_clock::now();
  new_servant->expires_tp = new_servant->discovered_tp + expire_time;

  // 任务数默认为0
  if (servant_info.observed_location != servant_info.reported_location) {
    LOG_WARN("发现新的NAT节点，报告地址: '{}', 实际地址: '{}'", servant_info.reported_location, servant_info.observed_location);
  } else {
	LOG_INFO("发现新节点", servant_info.observed_location);
  }
}

std::vector<std::uint64_t> TaskDispatcher::NotifyServantRunningTasks(
    const std::string& servant_location, std::vector<RunningTask> tasks) {
  std::vector<std::uint64_t> task_grant_ids;

  // 将tasks中的task id插入到结果集中
  std::transform(tasks.begin(), tasks.end(),
                 std::back_insert_iterator(task_grant_ids),
				 [](const RunningTask& t) {
	               return t.task_grant_id();
				 });

  std::scoped_lock _(alloc_mutex_);
  // 找到目标节点
  Servant::Ptr servant = nullptr;
  for (auto&& e : servants_) {
	if (e->servant_info.observed_location == servant_location) {
	  servant = e;
	  break;
	}
  }

  // 节点已经过期
  if (!servant) {
	return task_grant_ids;
  }

  // 清除僵尸任务
  UnsafeClearZombies(servant, {task_grant_ids.begin(), task_grant_ids.end()});

  // 找到对应节点允许运行的任务
  std::unordered_set<std::uint64_t> permitted_task_ids;
  for (auto&& [id, task] : tasks_) {
	if (task.servant == servant && !task.is_zombie) {
	  permitted_task_ids.insert(id);
	}
  }

  // 找到请求中未被允许的未知任务
  std::vector<std::uint64_t> unknown_tasks;
  for (auto it = tasks.begin(); it != tasks.end(); ) {
    if (!permitted_task_ids.count(it->task_grant_id())) {
      unknown_tasks.push_back(it->task_grant_id());
	  LOG_INFO("节点 '{}' 报告了一个未知的任务 '{}'", servant->servant_info.observed_location, it->task_grant_id());
      it = tasks.erase(it);
    } else {
      ++it;
    }
  }

  running_task_bookkeeper_.SetRunningTask(servant_location, std::move(tasks));
  return unknown_tasks;
}

std::vector<RunningTask> TaskDispatcher::GetRunningTasks() const {
	return running_task_bookkeeper_.GetRunningTasks();
}

void TaskDispatcher::UnsafeFreeTask(const std::vector<std::uint64_t> &task_ids) {
  // 遍历要删除的任务编号
  for (auto &&id : task_ids) {
  	// 找到要删除的任务
  	auto iter = tasks_.find(id);
  	if (iter == tasks_.end()) {
  		LOG_WARN("正在释放未知的任务 '{}'", id);
  		return;
  	}
  	--iter->second.servant->running_tasks; // 减少所分配节点的任务数
  	DISTBU_CHECK(tasks_.erase(id) == 1);   // 从任务表中删除
  }
  alloc_cv_.notify_all();
}

void TaskDispatcher::OnTimerExpiration(Poco::Timer& timer) {
  auto now = std::chrono::steady_clock::now();
//   LOG_DEBUG("定时器触发 OnTimerExpiration");

  std::scoped_lock _(alloc_mutex_);
  // 过期节点直接移除
  for (auto iter = servants_.begin(); iter != servants_.end(); ) {
    if ((*iter)->expires_tp < now) {
	  LOG_INFO("移除超时节点：'{}'", (*iter)->servant_info.observed_location);
	  running_task_bookkeeper_.DelServant((*iter)->servant_info.observed_location);
	  iter = servants_.erase(iter);
	} else {
	  ++iter;
	}
  }

  // 清除过期节点所属的任务
  {
    std::vector<std::uint64_t> clearing_task_ids;
	std::unordered_set<Servant*> alive_servants;
	for (auto servant : servants_) {
	  alive_servants.insert(servant.get());
	}
	for (auto&& [id, task] : tasks_) {
      if (alive_servants.count(task.servant.get()) == 0) {
        clearing_task_ids.push_back(id);
	  }
	}
	if (!clearing_task_ids.empty()) {
	  LOG_INFO("移除 {} 个任务", clearing_task_ids.size());
	}
	UnsafeFreeTask(clearing_task_ids);
  }

  // 过期任务标记为僵尸任务
  for (auto iter = tasks_.begin(); iter != tasks_.end(); ++iter) {
	if (iter->second.expires_tp < now) {
	  iter->second.is_zombie = true;
	  LOG_INFO("任务 '{}' 超时，", iter->first);
	}
  }
}

} // namespace distribuild::scheduler