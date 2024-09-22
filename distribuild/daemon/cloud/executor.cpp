#include <thread>
#include <coroutine>
#include <signal.h>
#include <sys/wait.h>
#include <Poco/ThreadPool.h>
#include <Poco/TaskManager.h>
#include "daemon/cloud/executor.h"
#include "common/spdlogging.h"
#include "daemon/sysinfo.h"
#include "daemon/cloud/excute.h"
#include "daemon/config.h"
#include "daemon/cloud/temp_dir.h"

using namespace std::literals;

namespace distribuild::daemon::cloud {

Executor* Executor::Instance() {
  static Executor instance;
  return &instance;
}

Executor::Executor()
  : timer_(0, 1'000)
  , task_manager_(Poco::ThreadPool::defaultPool()) {
  // 配置最大内存
  auto memory = ParseMemorySize(FLAGS_min_memory_for_starting_new_task);
  DISTBU_CHECK(memory > 0);
  min_memory_ = memory;

  // 配置并发数
  // ! 核心数对应可能2个线程？使用实际并发数?
  max_concurrency_ = 0;
  auto num_cpu_cores = GetNumCPUCores();
  LOG_INFO("节点类型：{}", FLAGS_servant_priority);
  if (FLAGS_max_concurrency == -1) {
    if (FLAGS_servant_priority == "dedicated") {
	  // 95%
	  max_concurrency_ = num_cpu_cores * 95 / 100;
	} else if (FLAGS_servant_priority == "user") {
	  // ! 是否在容器环境中
      if (num_cpu_cores < FLAGS_poor_cpu_cores_threshold) {
		LOG_INFO("机器性能不足，不会分配任务");
	  } else {
		// 40%
        max_concurrency_ = num_cpu_cores * 40 / 100;
	  }
	} else {
	  max_concurrency_ = 0;
	}
  } else {
	max_concurrency_ = FLAGS_max_concurrency;
  }

  if (max_concurrency_ > 0) {
	LOG_INFO("节点最多可同时处理 {} 个任务", max_concurrency_);
  } else {
	LOG_INFO("本机不会接受任务");
  }

  // 放入线程
  waitpid_worker_ = std::thread(std::bind(&Executor::WaiterProc, this));

  LOG_DEBUG("启动定时器 OnTimerClean");
  timer_.start(Poco::TimerCallback<Executor>(*this, &Executor::OnTimerClean));
}

Executor::~Executor() {
  exiting_.store(true);
  waitpid_semaphore_.release(1);
  if (waitpid_worker_.joinable())
    waitpid_worker_.join();
}

size_t Executor::GetMaxConcurrency() const {
  return max_concurrency_;
}

std::optional<std::uint64_t> Executor::TryQueueTask(std::uint64_t grant_id, std::shared_ptr<Task> user_task) {
  std::scoped_lock lock(task_mutex_);

  // 当前无法入队任务
  auto task_id = TryStartingNewTaskUnsafe();
  if (!task_id) {
	return std::nullopt;
  }

  auto cmd = user_task->GetCmdLine();

  // 放入任务表中
  auto&& task_desc = tasks_[*task_id];
  task_desc = std::make_shared<TaskDesc>();

  // 设置输入和临时输出
  TempFile temp_stdin(GetTempDir());
  temp_stdin.Write(user_task->GetSource());
  task_desc->std_out.emplace(GetTempDir());
  task_desc->std_err.emplace(GetTempDir());

  // 执行程序
  LOG_TRACE("执行命令: `{}`", cmd);
  pid_t pid = StartProgram(cmd, 5, temp_stdin.Fd(), task_desc->std_out->Fd(), task_desc->std_err->Fd(), true); // 父进程立即返回
  task_desc->grant_id = grant_id;
  task_desc->ref_count = 1;
  task_desc->start_tp = std::chrono::steady_clock::now();
  task_desc->pid = pid;
  task_desc->task = std::move(user_task);
  task_desc->cmd = cmd;

  // 唤醒等待线程
  waitpid_semaphore_.release();

  return task_id;
}

bool Executor::TryAddTaskRef(std::uint64_t task_id) {
  std::scoped_lock lock(task_mutex_);
  if (auto task = tasks_.find(task_id); task != tasks_.end()) {
    ++task->second->ref_count;
	return true;
  } else {
	return false;
  }
}

/// @brief 等待任务完成
std::pair<std::shared_ptr<Task>, ExecutionStatus> Executor::WaitForTask(std::uint64_t task_id, std::chrono::milliseconds timeout_ms) {
  std::shared_ptr<TaskDesc> task;
  // 寻找任务
  {
	std::scoped_lock lock(task_mutex_);
	if (auto iter = tasks_.find(task_id); iter != tasks_.end()) {
      task = iter->second;
	}
  }

  // 不存在
  if (!task) {
	return {nullptr, ExecutionStatus::NotFound};
  }

  // 等待任务
  if (!task->completion_event.tryWait(timeout_ms.count())) {
	return {nullptr, ExecutionStatus::Running};
  }

  return {task->task, ExecutionStatus::Unknown};
}

void Executor::FreeTask(std::uint64_t task_id) {
  std::shared_ptr<TaskDesc> free_task;

  {
	std::scoped_lock lock(task_mutex_);
	auto task = tasks_.find(task_id);
	// 没找到
	if (task == tasks_.end()) {
	  return;
	}
	// 其它客户端正在等待
    if (--task->second->ref_count > 0) {
	  return;
	}
	// 没人等待则清理
	free_task = task->second;
	tasks_.erase(task);
  }

  LOG_DEBUG("删除进程 cloud task id = {}", task_id);
  KillTask(free_task.get());
}

std::vector<Executor::RunningTask> Executor::GetAllTasks() const {
  std::vector<Executor::RunningTask> result;
  std::scoped_lock lock(task_mutex_);
  for (auto&& [k, v] : tasks_) {
	if (v->task) {
	  result.emplace_back(RunningTask{k, v->grant_id, v->task});
	}
  }
  return result;
}

void Executor::KillExpiredTasks(const std::unordered_set<std::uint64_t>& expired_grant_ids) {
  auto killed = 0;
  {
	std::scoped_lock lock(task_mutex_);
	for (auto&& [k, v] : tasks_) {
	  // 正在执行
	  if (v->is_running.load(std::memory_order_relaxed) &&
	      expired_grant_ids.count(v->grant_id)) {
	    KillTask(v.get());
		++killed;
	  }
	}
  }
  if (killed) {
	LOG_WARN("杀死 {} 个过期任务", killed);
  }
}

void Executor::Stop() {
  exiting_.store(true, std::memory_order_relaxed);

  timer_.stop();
  {
	std::scoped_lock lock(task_mutex_);
	for (auto&& [k, v] : tasks_) {
		KillTask(v.get());
	}
  }

  waitpid_semaphore_.release(); // 通知线程退出
}

void Executor::Join() {
  waitpid_worker_.join();
  while (true) {
	Poco::Thread::sleep(100);
	bool keep_sleep = false;
    // 如果全部都停止了则退出
	std::scoped_lock lock(task_mutex_);
	for (auto&& [_, v] : tasks_) {
	  if (v->is_running.load(std::memory_order_relaxed)) {
		keep_sleep = true;
		break;
	  }
	}
	if (!keep_sleep) {
	  break;
	}
  }
}

std::optional<std::uint64_t> Executor::TryStartingNewTaskUnsafe() {
  // 退出中...
  if (exiting_.load(std::memory_order_relaxed)) {
	return std::nullopt;
  }

  std::uint64_t task_id = next_task_id_++;

  // 任务数上限判断
  if (running_tasks_.fetch_add(1, std::memory_order_relaxed) + 1 > max_concurrency_) {
	LOG_WARN("任务数不够，主动拒绝");
	running_tasks_.fetch_sub(1, std::memory_order_relaxed);
	return std::nullopt;
  }

  // 内存
  if (GetAvailMemory() < min_memory_) {
    LOG_WARN("内存不足，主动拒绝");
	running_tasks_.fetch_sub(1, std::memory_order_relaxed);
	return std::nullopt;
  }

  task_ran_sum_.fetch_add(1, std::memory_order_relaxed);
  return task_id;
}

void Executor::KillTask(TaskDesc* task) {
  if (task && task->is_running.load(std::memory_order_relaxed)) {
	kill(-task->pid, SIGKILL);
  }
}

void Executor::OnTimerClean(Poco::Timer& timer) {
  auto now = std::chrono::steady_clock::now();
  std::vector<std::shared_ptr<TaskDesc>> free_tasks;

  {
	std::scoped_lock lock(task_mutex_);
	for (auto iter = tasks_.begin(); iter != tasks_.end(); ) {
	  // 完成后任务只允许保留1min，超过则删除
	  if (!iter->second->is_running.load(std::memory_order_relaxed) &&
	      iter->second->completed_tp + 1min < now) {
        free_tasks.push_back(iter->second);
		iter = tasks_.erase(iter);
	  } else {
		++iter;
	  }
	}
  }

  if (!free_tasks.empty()) {
	LOG_WARN("删除了 {} 个完成的任务", free_tasks.size());
  }
}

void Executor::OnExitCallback(pid_t pid, int exit_code) {
  LOG_DEBUG("开始ExitTask任务, pid = {}, exit_code = {}", pid, exit_code);

  // 竞态
  std::unique_lock lock(task_mutex_);
  // 寻找任务
  std::shared_ptr<TaskDesc> task;
  for (auto&& [k, v] : tasks_) {
	if (v->pid == pid) {
	  task = v;
	  break;
	}
  }
  // 完成
  running_tasks_.fetch_sub(1, std::memory_order_relaxed);
  // 未找到
  if (!task) {
	LOG_WARN("没有找到进程：pid = {}", pid);
	return;
  }
  // 找到
  if (exit_code == -1) {
	LOG_WARN("命令错误：{}", task->cmd);
  }
  task->completed_tp = std::chrono::steady_clock::now();
  task->is_running.store(false, std::memory_order_relaxed);
  // 不占用锁
  auto std_out = task->std_out->ReadAll();
  auto std_err = task->std_err->ReadAll();
  // 执行出错
  lock.unlock();
  task->task->OnCompleted(exit_code, std::move(std_out), std::move(std_err));
  task->completion_event.set(); // 通知完成
  LOG_INFO("任务执行完成");
}

void Executor::WaiterProc() {
  LOG_DEBUG("启动WaiterProc线程");

  auto has_work = [&] {
    // 执行器未处于退出状态且仍有任务在运行
	return !exiting_.load(std::memory_order_relaxed) ||
         running_tasks_.load(std::memory_order_relaxed);
  };

  while (has_work()) {
    waitpid_semaphore_.acquire(); // 等待唤醒（退出/程序执行完毕）
	if (!has_work()) break; // 退出

    // 获取子进程状态
	int status;
	pid_t pid = wait(&status); // 阻塞，等待一个终止的子进程

	if (pid == -1 && exiting_.load(std::memory_order_relaxed)) {
	  break;
	}

	DISTBU_CHECK_FORMAT(pid != -1, "等待子进程失败");

	int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	if (exit_code == -1) {
	  LOG_WARN("子进程 {} 非正常退出: {}", pid, exit_code);
	}

	task_manager_.start(new OnExitTask(this, pid, exit_code));
  }

  LOG_DEBUG("退出WaiterProc线程");
}

}  // namespace distribuild::daemon::cloud