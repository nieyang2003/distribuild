#include "task_run_keeper.h"
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include "common/logging.h"
#include "daemon/config.h"

using namespace std::literals;

namespace distribuild::daemon::local {

TaskRunKeeper::TaskRunKeeper() 
  : timer_(0, 1'000) {
  auto channel = grpc::CreateChannel(FLAGS_scheduler_location, grpc::InsecureChannelCredentials());
  scheduler_stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(scheduler_stub_);

  last_update_time_ = std::chrono::steady_clock::now();
  LOG_INFO("启动定时器 OnTimerRefresh");
  timer_.start(Poco::TimerCallback<TaskRunKeeper>(*this, &TaskRunKeeper::OnTimerRefresh));
}

TaskRunKeeper::~TaskRunKeeper() {}

std::optional<TaskRunKeeper::TaskDesc> TaskRunKeeper::TryFindTask(const std::string& task_digest) const {
  std::scoped_lock lock(mutex_);
  auto result = running_tasks_.find(task_digest);
  if (result != running_tasks_.end()) {
    return result->second;
  }
  return std::nullopt;
}

void TaskRunKeeper::Stop() {
  timer_.stop();
}

void TaskRunKeeper::Join() {}

void TaskRunKeeper::OnTimerRefresh(Poco::Timer& timer) {
  grpc::ClientContext context;
  scheduler::GetRunningTasksRequest  req;
  scheduler::GetRunningTasksResponse res;

  auto status = scheduler_stub_->GetRunningTasks(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("获取scheduler正在运行的任务失败");
	std::scoped_lock lock(mutex_);
    if (std::chrono::steady_clock::now() - last_update_time_ > 5s) {
	  LOG_INFO("超时，清除TaskRunKeeper");
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