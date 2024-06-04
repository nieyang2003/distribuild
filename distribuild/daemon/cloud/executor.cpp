#include "executor.h"

#include "distribuild/common/logging.h"
#include "distribuild/daemon/sysinfo.h"
#include "distribuild/daemon/cloud/excute.h"
#include "distribuild/daemon/cloud/config.h"
#include "distribuild/daemon/cloud/temp_dir.h"

#include <thread>
#include <coroutine>
#include <signal.h>
#include <sys/wait.h>

using namespace std::literals;

namespace distribuild::daemon::cloud {

Executor* Executor::Instance() {
  static Executor instance;
  return &instance;
}

Executor::Executor() {
  // 配置最大内存
  std::optional<std::size_t> memory = config::TryGetMemorySize();
  DISTBU_CHECK(memory);
  min_memory_ = *memory;

  // TODO: 配置并发数
  max_concurrency_  = std::thread::hardware_concurrency();
  if (max_concurrency_) {
	LOG_INFO("最多可同时处理 {} 个任务", max_concurrency_);
  }

  // TODO: 放入线程
  waitpid_worker_ = std::thread([this] { WaiterProc(); });

  // TODO: 创建定时器: OnTimerClean
}

std::pair<std::size_t, UnaccepteReason> Executor::GetMaxTasks() const {
  // TODO: 完善
  return std::pair<std::size_t, UnaccepteReason>(max_concurrency_, unaccepte_reason_);
}

std::optional<std::uint64_t> Executor::TryQueueTask(
    std::uint64_t grant_id, std::shared_ptr<Task> user_task) {
  std::scoped_lock lock(task_mutex_);

  // 当前无法入队任务
  auto task_id = TryStartingNewTaskUnsafe();
  if (!task_id) {
	return std::nullopt;
  }
  
  auto cmd = user_task->GetCmdLine();
  LOG_TRACE("Executing: {}", cmd);

  // 放入任务表中
  auto&& task = tasks_[*task_id];
  task = std::make_shared<TaskDesc>();

  // 设置输入和临时输出
  TempFile temp_stdin(GetTempDir());
  temp_stdin.Write(user_task->GetStdInput()); // TODO
  task->std_out.emplace(GetTempDir());
  task->std_err.emplace(GetTempDir());

  // 执行程序
  pid_t pid = StartProgram(cmd, 5, temp_stdin.Fd(), task->std_out->Fd(), task->std_err->Fd(), true); // 父进程立即返回
  task->grant_id = grant_id;
  task->ref_count = 1;
  task->start_tp = std::chrono::steady_clock::now();
  task->pid = pid;
  task->task = std::move(user_task);
  task->cmd = cmd;

  // 唤醒等待线程
  waitpid_semaphore_.release();

  return task_id;
}

bool Executor::TryAddTaskRef(std::uint64_t task_id) {
  std::scoped_lock lock(task_mutex_);
  if (auto task = tasks_.find(task_id); task != tasks_.end()) {
	return false;
  } else {
	++task->second->ref_count;
	return true;
  }
}

/// @brief 等待任务完成
std::pair<std::shared_ptr<Task>, ExecutionStatus> Executor::WaitForTask(
    std::uint64_t task_id, std::chrono::nanoseconds timeout) {
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

  // 粗暴的等待超时
  // TODO: 协程异步
  int t = (timeout + 999ns) / 1000ns;
  while (!task->compile_latch.try_wait() && t--) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }

  if (t > 0)
    return {task->task, ExecutionStatus::Unknown};
  else
  	return {nullptr, ExecutionStatus::Running};
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
	  if (v->running.load(std::memory_order_relaxed) &&
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

  // TODO: 关闭定时器
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
	// TODO: 协程等待100ms
	bool keep_sleep = false;
    // 如果全部都停止了则退出
	std::scoped_lock lock(task_mutex_);
	for (auto&& [_, v] : tasks_) {
	  if (v->running.load(std::memory_order_relaxed)) {
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
  if (GetMemoryAvailable() < min_memory_) {
    LOG_WARN("内存不足，主动拒绝");
	running_tasks_.fetch_sub(1, std::memory_order_relaxed);
	return std::nullopt;
  }

  task_ran_.fetch_add(1, std::memory_order_relaxed);
  return task_id;
}

void Executor::KillTask(TaskDesc* task) {
  if (task && task->running.load(std::memory_order_relaxed)) {
	kill(-task->pid, SIGKILL);
  }
}

void Executor::OnTimerClean() {
  auto now = std::chrono::steady_clock::now();
  std::vector<std::shared_ptr<TaskDesc>> free_tasks;

  {
	std::scoped_lock lock(task_mutex_);
	for (auto iter = tasks_.begin(); iter != tasks_.end(); ) {
	  // 完成后任务只允许保留1min，超过则删除
	  if (!iter->second->running.load(std::memory_order_relaxed) &&
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
	LOG_WARN("没有找到进程：{}", pid);
	return;
  }
  // 找到
  if (exit_code == -1) {
	LOG_WARN("命令错误：{}", task->cmd);
  }
  task->completed_tp = std::chrono::steady_clock::now();
  task->running.store(false, std::memory_order_relaxed);
  // 执行出错
  lock.unlock();
  task->task->OnCompleted(exit_code, std::move(task->std_out->ReadAll()), std::move(task->std_err->ReadAll()));
  task->compile_latch.count_down();
}

void Executor::WaiterProc() {
  auto has_work = [&] -> bool {
    // 执行器未处于退出状态且仍有任务在运行
	return !exiting_.load(std::memory_order_relaxed) ||
         running_tasks_.load(std::memory_order_relaxed);
  };

  while (has_work()) {
    waitpid_semaphore_.acquire(); // 等待唤醒（退出/程序执行完毕）
	if (!has_work()) break; // 因为退出

    // 获取子进程状态
	int status;
	pid_t pid = wait(&status); // 阻塞，等待一个终止的进程

	if (pid == -1 && exiting_.load(std::memory_order_relaxed)) {
	  break;
	}

	DISTBU_CHECK(pid != -1, "等待子进程失败");
    
	int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	if (exit_code == -1) {
		LOG_WARN("子进程 {} 非正常退出: {}", pid, exit_code);
	}

	OnExitCallback(pid, exit_code); // TODO: 放入异步运行时
  }
}

}  // namespace distribuild::daemon::cloud