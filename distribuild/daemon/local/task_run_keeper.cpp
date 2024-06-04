#include "task_run_keeper.h"

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>

#include "distribuild/common/logging.h"

using namespace std::literals;

namespace distribuild::daemon::local {

TaskRunKeeper::TaskRunKeeper() {
  auto channel = grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials());
  auto scheduler_stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(scheduler_stub_);

  // TODO: 设置每秒fresh
  last_update_time_ = std::chrono::steady_clock::now();
}

TaskRunKeeper::~TaskRunKeeper() {}

std::optional<TaskRunKeeper::TaskDesc> TaskRunKeeper::TryFindTask(
    const std::string& task_digest) const {
  std::scoped_lock lock(mutex_);
  auto result = running_tasks_.find(task_digest);
  if (result != running_tasks_.end()) {
    return result->second;
  }
  return std::nullopt;
}

void TaskRunKeeper::Stop() {
  // TODO: 停止计时器
}

void TaskRunKeeper::Join() {}

void TaskRunKeeper::OnTimerRefresh() {
  grpc::ClientContext context;
  scheduler::GetRunningTasksRequest  req;
  scheduler::GetRunningTasksResponse res;

  auto status = scheduler_stub_->GetRunningTasks(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("向scheduler同步正在运行的任务失败");
	std::scoped_lock lock(mutex_);
    if (std::chrono::steady_clock::now() - last_update_time_ > 5s) {
      running_tasks_.clear();
    }
    return;
  }

  std::unordered_map<std::string, TaskDesc> tmp_running_tasks;
  for (auto&& task : res.running_tasks()) {
	TaskDesc task_desc{
	  .servant_location = task.servant_location(),
	  .servant_task_id  = task.servant_task_id(),
	};
	tmp_running_tasks[task.task_digest()] = std::move(task_desc);
  }

  std::scoped_lock lock(mutex_);
  last_update_time_ = std::chrono::steady_clock::now();
  running_tasks_.swap(tmp_running_tasks);
}

}