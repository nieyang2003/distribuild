#include "task_dispatcher.h"

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <sys/stat.h>

#include "distribuild/common/logging.h"
#include "distribuild/daemon/local/cache_reader.h"
#include "distribuild/daemon/version.h"

using namespace std::literals;

namespace distribuild::daemon::local {

namespace {

std::uint64_t NextTaskId() {
  static std::atomic<std::uint64_t> next_id{};
  return ++next_id;
}

bool IsProcAlive(pid_t pid) {
  struct stat buf;
  return lstat(fmt::format("/proc/{}/", pid).c_str(), &buf) == 0;
}

} // namespace

TaskDispatcher* TaskDispatcher::Instance() {
  static TaskDispatcher instance;
  return &instance;
}

distribuild::daemon::local::TaskDispatcher::TaskDispatcher() {
  auto channel = grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials());
  auto scheduler_stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(scheduler_stub_);

  // TODO: 启动定时器
}

TaskDispatcher::~TaskDispatcher() {
  // TODO: 取消定时器
}

std::uint64_t TaskDispatcher::QueueTask(std::unique_ptr<DistTask> task, std::chrono::steady_clock::time_point start_deadline) {
  auto task_desc = std::make_shared<TaskDesc>();

  task_desc->task_id        = NextTaskId();
  task_desc->state          = TaskDesc::State::Pending;
  task_desc->task           = std::move(task);
  task_desc->start_tp       = std::chrono::steady_clock::now();
  task_desc->start_deadline = start_deadline;

  {
    std::scoped_lock lock(tasks_mutex_);
    tasks_[task_desc->task_id] = task_desc;
  }

  // TODO: 异步启动一个Task
  PerformTask(task_desc);
  
  return task_desc->task_id;
}

std::pair<std::unique_ptr<DistTask>, TaskDispatcher::WaitStatus> TaskDispatcher::WaitForTask(
    std::uint64_t task_id, std::chrono::nanoseconds timeout) {
  std::shared_ptr<TaskDesc> task_desc;
  {
	std::scoped_lock lock(tasks_mutex_);
	if (auto iter = tasks_.find(task_id); iter != tasks_.end()) {
	  task_desc = iter->second;
	}
  }
  if (!task_desc) {
	return {nullptr, WaitStatus::NotFound};
  }
  // TODO: 更好的等待实现
  while (timeout > 0s && !task_desc->completed_latch.try_wait()) {
    timeout -= 1s;
	std::this_thread::sleep_for(1s);
  }
  if (timeout <= 0s) {
	return {nullptr, WaitStatus::Timeout};
  }

  {
	std::scoped_lock lock(tasks_mutex_);
	tasks_.erase(task_id);
  }
  std::scoped_lock lock(task_desc->mutex);
  return {std::move(task_desc->task), WaitStatus::OK};
}

void TaskDispatcher::Stop() {
  // TODO: 取消定时器

  task_grant_keeper_.Stop();
  config_keeper_.Stop();
  task_run_keeper_.Stop();
}

void TaskDispatcher::Join() {
  task_grant_keeper_.Join();
  config_keeper_.Join();
  task_run_keeper_.Join();
}

void distribuild::daemon::local::TaskDispatcher::PerformTask(std::shared_ptr<TaskDesc> task_desc) {
  {
	std::scoped_lock lock(task_desc->mutex);
	task_desc->output.exit_code = -114;
  }
  
  std::shared_ptr<void> deffer(nullptr, [&]{
    std::scoped_lock lock(task_desc->mutex);
	task_desc->task->OnCompleted(task_desc->output);
	task_desc->state = TaskDesc::State::Done;
	task_desc->completed_tp = std::chrono::steady_clock::now();
	task_desc->completed_latch.count_down();

	LOG_INFO("任务 `{}` 编译完成", task_desc->task_id);
  });
  
  // 查缓存查看是否有结果
  if (TryReadCache(task_desc.get())) {
	hit_cache_.fetch_add(1, std::memory_order_relaxed);
    return;
  };

  // 查看任务是否正在运行
  if (TryGetExistedResult(task_desc.get())) {
	existed_times.fetch_add(1, std::memory_order_relaxed);
	return;
  }
  
  // 的确不存在，启动新任务
  StartNewServantTask(task_desc.get());

  run_times_.fetch_add(1, std::memory_order_relaxed);
}

bool TaskDispatcher::TryReadCache(TaskDesc* task_desc) {
  auto cache_entry = task_desc->task->CacheControl() ?
  					 CacheReader::Instance()->TryRead(task_desc->task->CacheKey()) :
					 std::nullopt;
  if (cache_entry) { // 命中缓存
	task_desc->output = DistTask::DistOutput {
        .exit_code = 0,
	    .std_out = cache_entry->std_out,
	    .std_err = cache_entry->std_err,
	    .output_files = std::move(cache_entry->output_files),
	  }; // TODO: 所有权优化
	return true;
  }
  return false;
}

bool TaskDispatcher::TryGetExistedResult(TaskDesc* task_desc) {
  // 查找正在运行的任务，没有返回false
  auto running_task = task_run_keeper_.TryFindTask(task_desc->task->GetDigest());
  if (!running_task) {
	return false;
  }
  
  // 创建grpc请求及相应
  auto stub = cloud::DaemonService::NewStub(
		grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials()));
  grpc::ClientContext context;
  cloud::AddTaskRefRequest  addRefReq;
  cloud::AddTaskRefResponse addRefRes;
  addRefReq.set_token(config_keeper_.GetServingDaemonToken());
  addRefReq.set_task_id(running_task->servant_task_id);
  
  // 发起rpc请求
  auto status = stub->AddTaskRef(&context, addRefReq, &addRefRes);
  if (!status.ok()) {
	LOG_WARN("RPC请求失败：{}", status.error_details());
	return false;
  }

  // 存在则更新状态
  {
	std::scoped_lock lock(task_desc->mutex);
	task_desc->last_keep_alive_tp = std::chrono::steady_clock::now();
	task_desc->ready_tp = std::chrono::steady_clock::now();
	task_desc->dispatched_tp = std::chrono::steady_clock::now();
	task_desc->task_grant_id = 0;
	task_desc->servant_location = running_task->servant_location;
	task_desc->servant_task_id = running_task->servant_task_id;
    task_desc->state = TaskDesc::State::Dispatched;
  }

  // 请求等待
  WaitServantTask(stub.get(), task_desc);

  // 退出
  FreeServantTask(stub.get(), task_desc->servant_task_id);
  return true;
}

void TaskDispatcher::StartNewServantTask(TaskDesc* task_desc) {
  std::optional<TaskGrantKeeper::GrantDesc> task_grant;
  while (!task_grant && !task_desc->aborted.load(std::memory_order_relaxed)) {
	task_grant = task_grant_keeper_.Get(task_desc->task->EnviromentDesc(), 1s);
  }

  if (!task_grant) {
	LOG_ERROR("创建新任务失败，任务`{}`被中止", task_desc->task_id);
	return;
  }

  // 更新状态
  {
	std::scoped_lock lock(task_desc->mutex);
	task_desc->last_keep_alive_tp = std::chrono::steady_clock::now();
	task_desc->ready_tp = std::chrono::steady_clock::now();
	task_desc->task_grant_id = task_grant->grant_id;
	task_desc->servant_location = task_grant->servant_location;
    task_desc->state = TaskDesc::State::Ready;
  }

  // rpc通道
  auto stub = cloud::DaemonService::NewStub(
		grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials()));

  auto task_id = task_desc->task->StartTask(stub.get(), 
  	config_keeper_.GetServingDaemonToken(), task_grant->grant_id);
  if (!task_id) {
	LOG_ERROR("提交任务失败");
	task_grant_keeper_.Free(task_grant->grant_id);
	return;
  }

  // 更新状态
  {
	std::scoped_lock lock(task_desc->mutex);
	task_desc->dispatched_tp = std::chrono::steady_clock::now();
	task_desc->state = TaskDesc::State::Dispatched;
	task_desc->servant_task_id = *task_id;
  }

  // 等待任务执行
  WaitServantTask(stub.get(), task_desc);

  FreeServantTask(stub.get(), task_desc->servant_task_id);
  task_grant_keeper_.Free(task_grant->grant_id);
  return;
}

void TaskDispatcher::WaitServantTask(cloud::DaemonService::Stub* stub, TaskDesc* task_desc) {
  const auto kRetries = 5;

  auto retries = kRetries;
  while (retries-- && task_desc->aborted.load(std::memory_order_relaxed)) {
	auto result = WaitServantTask(stub, task_desc->servant_task_id);
    
	// 失败
	if (!result.first) {
      if (result.second == 1) { // rpc
        LOG_WARN("RPC错误， 剩余重试次数{}，task_id：{}，节点地址：{}", retries, task_desc->task_id, task_desc->servant_location);
		continue;
	  } else if (result.second == 2) { // running
        retries = kRetries; // 正在运行：重新等待
	  } else if (result.second == 3) { // failed
        std::scoped_lock lock(task_desc->mutex);
		task_desc->output.exit_code = -125;
	  } else {
		DISTBU_CHECK(false, "不可达错误");
	  }
	}

	// 成功
	if (result.first->exit_code == 127) {
	  LOG_WARN("无法找到编译器，节点：{}，stderr：{}", task_desc->servant_location, result.first->std_err);
	}

	std::scoped_lock lock(task_desc->mutex);
	task_desc->output = *result.first;
	break;
  }
}

std::pair<std::optional<DistTask::DistOutput>, int> TaskDispatcher::WaitServantTask(
    cloud::DaemonService::Stub* stub, std::uint64_t servant_task_id) {
  grpc::ClientContext    context;
  cloud::WaitForTaskRequest  req;
  cloud::WaitForTaskResponse res;

  req.set_version(DISTRIBUILD_VERSION);
  req.set_token(config_keeper_.GetServingDaemonToken());
  req.set_task_id(servant_task_id);
  req.set_wait_ms(2s / 1ms);
  req.add_acceptable_compress_types(cloud::CompressType::COMPRESS_TYPE_ZSTD);
  context.set_deadline(30s);

  auto status = stub->WaitForTask(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("RPC `WaitForTask` 调用失败");
    return {std::nullopt, 1}; // 1: rpc error
  }

  if (res.task_status() == cloud::TaskStatus::TASK_STATUS_RUNNING) {
	return {std::nullopt, 2}; // 2: running error
  } else if (res.task_status() == cloud::TaskStatus::TASK_STATUS_DONE) {
    DistTask::DistOutput output {
      .exit_code = res.exit_code(),
	  .std_out   = res.output(),
	  .std_err   = res.err(),
	  // TODO: 
	};
	return {output, 0}; // 0: done
  } else {
	return {std::nullopt, 3}; // 3: failed
  }
}

void TaskDispatcher::FreeServantTask(cloud::DaemonService::Stub* stub, std::uint64_t servant_task_id) {
  grpc::ClientContext context;
  cloud::FreeTaskRequest  req;
  cloud::FreeTaskResponse res;

  req.set_token(config_keeper_.GetServingDaemonToken());
  req.set_task_id(servant_task_id);

  stub->FreeTask(&context, req, &res);
}

// ----------------------------------------------------------------------- //

void TaskDispatcher::OnTimerTimeoutAbort() {
  std::size_t aborted = 0;

  {
	auto now = std::chrono::steady_clock::now();
	std::scoped_lock lock(tasks_mutex_);
	for (auto&& [_, v] : tasks_) {
	  if (v->start_deadline < now) {  // 超时
		v->aborted.store(true, std::memory_order_relaxed);
		++aborted;
	  }
	}
  }

  if (aborted) {
	LOG_WARN("停止了{}个任务", aborted);
  }
}

void TaskDispatcher::OnTimerKeepAlive() {
  auto now = std::chrono::steady_clock::now();
  std::vector<std::uint64_t> task_ids;
  grpc::ClientContext context;
  scheduler::KeepTaskAliveRequest  req;
  scheduler::KeepTaskAliveResponse res;
  
  req.set_token("123456");
  {
	std::scoped_lock lock1(tasks_mutex_);
	for (auto&& [k, v] : tasks_) {
	  std::scoped_lock lock2(v->mutex);

      // 是否完成或未启动
	  if (v->state == TaskDesc::State::Pending ||
	      v->state == TaskDesc::State::Done) {
	    continue;
	  }
      
	  // 是否已被中止
      if (v->aborted.load(std::memory_order_relaxed)) {
		continue;
	  }

      // 长时间未keep-alive
	  if (now - v->last_keep_alive_tp > 1min) {
		v->aborted.store(true, std::memory_order_relaxed);
	  }

	  // 
	  req.add_task_grant_ids(v->task_grant_id);
	  task_ids.push_back(v->task_id);
	}
  }
  req.set_next_keep_alive_in_ms(10s / 1ms);

  // 不需要keep alive
  if (req.task_grant_ids().empty()) {
	return;
  }

  // rpc
  context.set_deadline(now + 5s);
  auto status = scheduler_stub_->KeepTaskAlive(&context, req, &res);
  if (!status.ok() || res.statues_size() != req.task_grant_ids_size()) {
	LOG_WARN("RPC调用KeepTaskAlive失败");
  } else {
    std::scoped_lock lock1(tasks_mutex_);
	for (auto i = 0; i < task_ids.size(); ++i) {
	  if (res.statues(i)) {
		if (auto iter = tasks_.find(task_ids[i]); iter != tasks_.end()) {
          std::scoped_lock lock2(iter->second->mutex);
		  iter->second->last_keep_alive_tp = now; // 更新alive时间
		}
	  } else {
		LOG_WARN("Keep task `{}` alive 失败", task_ids[i]);
	  }
	}
  }
}

void TaskDispatcher::OnTimerKilledAbort() {
  std::size_t aborted = 0;

  {
	auto now = std::chrono::steady_clock::now();
	std::scoped_lock lock(tasks_mutex_);
	for (auto&& [_, v] : tasks_) {
	  if (!v->aborted.load(std::memory_order_relaxed) &&
	      IsProcAlive(v->task->GetRequesterPid())) { // 没有aborted但程序却不存在
		v->aborted.store(true, std::memory_order_relaxed);
		++aborted;
	  }
	}
  }

  if (aborted) {
	LOG_WARN("停止了{}个任务", aborted);
  }
}

void TaskDispatcher::OnTimerClear() {
  std::vector<std::shared_ptr<TaskDesc>> destroying;
  auto now = std::chrono::steady_clock::now();

  {
    std::scoped_lock lock(tasks_mutex_);
    for (auto iter = tasks_.begin(); iter != tasks_.end();) {
	  std::scoped_lock lock2(iter->second->mutex);

	  // 未完成，遍历下一个
	  if (iter->second->state != TaskDesc::State::Done) {
		++iter;
		continue;
	  }

      // 如果已经完成则删除
      if (iter->second->aborted.load(std::memory_order_relaxed)) {
		destroying.push_back(std::move(iter->second));
		iter = tasks_.erase(iter);
		continue;
	  }

	  // 太长时间没有启动
	  if (iter->second->start_deadline + 1min < now) {
		LOG_WARN("任务`{}`太长时间未启动", iter->second->task_id);
        destroying.push_back(std::move(iter->second));
		iter = tasks_.erase(iter);
		continue;
	  }

	  ++iter;
	}
  }

  // 释放
}

} // namespace distribuild::daemon::local