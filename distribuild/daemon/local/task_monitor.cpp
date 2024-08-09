#include <fstream>
#include <sstream>
#include <functional>
#include "daemon/config.h"
#include "daemon/local/task_monitor.h"
#include "common/logging.h"
#include "common/tools.h"

namespace distribuild::daemon::local {

namespace {

bool IsAliveProc(pid_t pid) {
  std::ifstream ifs(fmt::format("/proc/{}/status", pid));
  if (!ifs) return false;

  std::string str;
  while (std::getline(ifs, str)) {
	if (StartWith(str, "State:")) {
      std::stringstream ss(str);
	  std::string x, y;
	  ss >> x >> y;

	  // R  运行
      // S  在可中断等待中休眠
      // D  在不可中断的磁盘休眠中等待
      // Z  僵尸
      // T  已停止（收到信号）或（在 Linux 2.6.33 之前）已停止跟踪
      // t  跟踪停止（从 Linux 2.6.33 起）
      // W  页面化（仅在 Linux 2.6.0 之前）
      // X  死亡（从 Linux 2.6.0 开始）
      // x  死亡（仅在 Linux 2.6.33 到 3.13 之间）
      // K  唤醒终止（仅在 Linux 2.6.33 到 3.13 之间）
      // W  正在唤醒（仅在 Linux 2.6.33 到 3.13 之间）
      // P  停放（仅在 Linux 3.9 到 3.13 之间）
	  return y != "Z" && y != "x" && y != "X";
	}
  }

  LOG_FATAL("未找到进程 {}", pid);
  return false;
}

} // namespace

TaskMonitor* TaskMonitor::Instance() {
  static TaskMonitor instance;
  return &instance;
}

TaskMonitor::TaskMonitor()
  : timer_(0, 1'000) {
  if (FLAGS_max_concurrency > 0) {
	max_heavy_tasks_ = FLAGS_max_concurrency;
  } else {
	max_heavy_tasks_ = std::thread::hardware_concurrency();
  }
  max_light_tasks_ = max_heavy_tasks_ * 1.5;
  
  timer_.start(Poco::TimerCallback<TaskMonitor>(*this, &TaskMonitor::OnTimerCheckAliveProc));
}

TaskMonitor::~TaskMonitor() {
  timer_.stop();
}

bool TaskMonitor::WaitForNewTask(pid_t pid, bool lightweight, std::chrono::nanoseconds timeout) {
  // 增减当前任务数记录
  auto&& waiter = lightweight ? light_tasks_ : heavy_tasks_;
  waiter.fetch_add(1, std::memory_order_relaxed);
  auto deffer = std::unique_ptr<void, std::function<void(void*)>>(nullptr, [&] (void*) {
	waiter.fetch_sub(1, std::memory_order_relaxed);
  });

  // 等待有任务释放
  std::unique_lock lock(permission_mutex_);
  auto success = permission_cv_.wait_for(lock, timeout, [&] {
	// ! 当轻量级任务数过多时可能导致重量级任务持续无法进入而形成饥饿状态
    return permissions_granted_.size() < max_heavy_tasks_ + (lightweight ? max_light_tasks_ : 0);
  });

  // 任务释放，尝试添加
  if (success) [[likely]] {
	if (permissions_granted_.count(pid)) [[unlikely]] {
	  LOG_ERROR("添加重复的进程：{}", pid);
	  return true;
	}
	permissions_granted_.insert(pid);
  }
  return success;
}

void TaskMonitor::DropTask(pid_t pid) {
  {
	std::scoped_lock lock(permission_mutex_);
	if (permissions_granted_.erase(pid) == 0) [[unlikely]] {
	  LOG_ERROR("删除未知的进程：{}", pid);
	  return;
	}
  }
  permission_cv_.notify_all();
}

void TaskMonitor::OnTimerCheckAliveProc(Poco::Timer& timer) {
  std::scoped_lock lock(permission_mutex_);
  for (auto iter = permissions_granted_.begin(); iter != permissions_granted_.end(); ) {
	if (!IsAliveProc(*iter)) {
	  LOG_WARN("进程 {} 退出但没有通知", *iter);
	  iter = permissions_granted_.erase(iter);
	} else {
	  ++iter;
	}
  }
  permission_cv_.notify_all();
}

} // distribuild::daemon::local