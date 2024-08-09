#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <sys/stat.h>
#include "daemon/local/task_dispatcher.h"
#include "common/logging.h"
#include "common/tools.h"
#include "daemon/local/cache_reader.h"
#include "daemon/version.h"
#include "daemon/config.h"

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

TaskDispatcher::TaskDispatcher()
  : timer_timeout_abort_(0, 1'000)
  , timer_keep_alive_(0, 1'000)
  , timer_killed_abort_(0, 1'000)
  , timer_clear_(0, 1'000)
  , task_manager_(Poco::ThreadPool::defaultPool())
  , scheduler_stub_(scheduler::SchedulerService::NewStub(grpc::CreateChannel(FLAGS_scheduler_location, grpc::InsecureChannelCredentials()))) {
  DISTBU_CHECK(scheduler_stub_);

  LOG_INFO("启动定时器：OnTimerTimeoutAbort、OnTimerKeepAlive、OnTimerKilledAbort、OnTimerClear");
  timer_timeout_abort_.start(Poco::TimerCallback<TaskDispatcher>(*this, &TaskDispatcher::OnTimerTimeoutAbort));
  timer_keep_alive_.start(Poco::TimerCallback<TaskDispatcher>(*this, &TaskDispatcher::OnTimerKeepAlive));
  timer_killed_abort_.start(Poco::TimerCallback<TaskDispatcher>(*this, &TaskDispatcher::OnTimerKilledAbort));
  timer_clear_.start(Poco::TimerCallback<TaskDispatcher>(*this, &TaskDispatcher::OnTimerClear));
}

TaskDispatcher::~TaskDispatcher() {
  timer_timeout_abort_.stop();
  timer_keep_alive_.stop();
  timer_killed_abort_.stop();
  timer_clear_.stop();
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
	LOG_DEBUG("创建local task, id = {}", task_desc->task_id);
    tasks_[task_desc->task_id] = task_desc;
  }

  task_manager_.start(new PerformPocoTask(this, task_desc));

  return task_desc->task_id;
}

std::pair<std::unique_ptr<DistTask>, TaskDispatcher::WaitStatus> TaskDispatcher::WaitForTask(std::uint64_t task_id, std::chrono::milliseconds timeout) {
  std::shared_ptr<TaskDesc> task_desc;
  {
	std::scoped_lock lock(tasks_mutex_);
	if (auto iter = tasks_.find(task_id); iter != tasks_.end()) {
	  task_desc = iter->second;
	}
  }
  if (!task_desc) {
	LOG_DEBUG("未找到：local task id = {}", task_id);
	return {nullptr, WaitStatus::NotFound};
  }

  if (!task_desc->completion_event.tryWait(timeout.count())) {
	LOG_DEBUG("等待超时：local task id = {}", task_id);
	return {nullptr, WaitStatus::Timeout};
  }

  {
	std::scoped_lock lock(tasks_mutex_);
	LOG_DEBUG("删除local task, id = {}", task_desc->task_id);
	tasks_.erase(task_id);
  }
  std::scoped_lock lock(task_desc->mutex);
  return {std::move(task_desc->task), WaitStatus::OK};
}

void TaskDispatcher::Stop() {
  timer_timeout_abort_.stop();
  timer_keep_alive_.stop();
  timer_killed_abort_.stop();
  timer_clear_.stop();

  task_grant_keeper_.Stop();
  config_keeper_.Stop();
  task_run_keeper_.Stop();
}

void TaskDispatcher::Join() {
  task_grant_keeper_.Join();
  config_keeper_.Join();
  task_run_keeper_.Join();
}

void TaskDispatcher::PerformTask(std::shared_ptr<TaskDesc> task_desc) {
  LOG_DEBUG("开始Perform Task");
  {
	std::scoped_lock lock(task_desc->mutex);
	task_desc->output.exit_code = -114514;
  }

  auto deffer = std::unique_ptr<void, std::function<void(void*)>>((void*)1, [&] (void*) {
    std::scoped_lock lock(task_desc->mutex);
    task_desc->task->OnCompleted(std::move(task_desc->output));
    task_desc->state = TaskDesc::State::Done;
    task_desc->completed_tp = std::chrono::steady_clock::now();
	task_desc->completion_event.set();

    LOG_INFO("任务 `{}` 编译完成", task_desc->task_id);
  });

  // 查缓存查看是否有结果
  if (TryReadCache(task_desc.get())) {
    LOG_DEBUG("cache命中");
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
	auto files = TryUnpackFiles(cache_entry->files);
	if (!files) return false;
	task_desc->output = DistTask::DistOutput {
        .exit_code = 0,
	    .std_out = cache_entry->std_out,
	    .std_err = cache_entry->std_err,
		.extra_info = cache_entry->extra_info,
	    .output_files = std::move(*files),
	  };
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
  auto stub = cloud::DaemonService::NewStub(grpc::CreateChannel(task_desc->servant_location, grpc::InsecureChannelCredentials()));
  grpc::ClientContext context;
  cloud::AddTaskRefRequest  addRefReq;
  cloud::AddTaskRefResponse addRefRes;
  addRefReq.set_token(config_keeper_.GetServingDaemonToken());
  addRefReq.set_task_id(running_task->servant_task_id);
  
  // 发起rpc请求
  auto status = stub->AddTaskRef(&context, addRefReq, &addRefRes);
  if (!status.ok()) {
	LOG_WARN("RPC请求失败：{}", status.error_message());
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
	task_grant = task_grant_keeper_.Get(task_desc->task->GetEnviromentDesc(), 1s);
  }

  if (!task_grant) {
	LOG_ERROR("创建新任务失败，任务`{}`被中止", task_desc->task_id);
	return;
  }

  LOG_INFO("分发任务给节点：{}", task_grant->servant_location);
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
  auto stub = cloud::DaemonService::NewStub(grpc::CreateChannel(task_desc->servant_location, grpc::InsecureChannelCredentials()));

  auto task_id = task_desc->task->StartTask(stub.get(), config_keeper_.GetServingDaemonToken(), task_grant->grant_id);
  if (!task_id) {
	LOG_ERROR("提交任务失败");
	task_grant_keeper_.Free(task_grant->grant_id);
	return;
  }
  LOG_DEBUG("提交任务给cloud, cloud task id = {}", *task_id);

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
  while (retries-- && !task_desc->aborted.load(std::memory_order_relaxed)) {
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
		break;
	  } else {
		DISTBU_CHECK_FORMAT(false, "不可达错误");
	  }
	}

	// 成功
	if (result.first->exit_code == 127) {
	  LOG_WARN("无法找到编译器，节点：{}，stderr：{}", task_desc->servant_location, result.first->std_err);
	}

	std::scoped_lock lock(task_desc->mutex);
	LOG_DEBUG("编译完成");
	task_desc->output = *result.first;
	break;
  }
}

std::pair<std::optional<DistTask::DistOutput>, int> TaskDispatcher::WaitServantTask(
    cloud::DaemonService::Stub* stub, std::uint64_t servant_task_id) {
  grpc::ClientContext    context;
  cloud::WaitForTaskRequest  req;
  cloud::WaitForTaskResponse resp;
  cloud::WaitForTaskResponseChunk chunk;
  std::string file;

  req.set_version(DISTRIBUILD_VERSION);
  req.set_token(config_keeper_.GetServingDaemonToken());
  req.set_task_id(servant_task_id);
  req.set_wait_ms(2s / 1ms);
  req.add_acceptable_compress_types(cloud::CompressType::COMPRESS_TYPE_ZSTD);
  SetTimeout(&context, 30s);

  // 读取数据
  auto reader = stub->WaitForTask(&context, req);
  while (reader->Read(&chunk)) {
	if (chunk.has_response()) {
	  resp = chunk.response();
	}
	if (!chunk.file_chunk().empty()) {
	  file.append(chunk.file_chunk().data(), chunk.file_chunk().size());
	}
  }

  //读取完毕
  grpc::Status status = reader->Finish();
  if (!status.ok()) {
	LOG_WARN("RPC `WaitForTask` 调用失败：{}", status.error_message());
    return {std::nullopt, 1}; // 1: rpc error
  }

  if (resp.task_status() == cloud::TaskStatus::TASK_STATUS_RUNNING) {
	return {std::nullopt, 2}; // 2: running error
  } else if (resp.task_status() == cloud::TaskStatus::TASK_STATUS_DONE) {
    DistTask::DistOutput output {
      .exit_code = resp.exit_code(),
	  .std_out   = resp.output(),
	  .std_err   = resp.err(),
	  .extra_info = resp.extra_info(),
	};
	if (output.exit_code == 0) {
	  auto files = TryUnpackFiles(file);
	  if (!files) {
		LOG_ERROR("TryUnpackFiles 失败");
		return {std::nullopt, 3};
	  }
	  output.output_files = std::move(*files);
	}
	LOG_DEBUG("exit_code = {}, std out = {}, std err = {}", output.exit_code, output.std_out, output.std_err);
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

void TaskDispatcher::OnTimerTimeoutAbort(Poco::Timer& timer) {
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

void TaskDispatcher::OnTimerKeepAlive(Poco::Timer& timer) {
  auto now = std::chrono::steady_clock::now();
  std::vector<std::uint64_t> task_ids;
  grpc::ClientContext context;
  scheduler::KeepTaskAliveRequest  req;
  scheduler::KeepTaskAliveResponse resp;
  
  req.set_token(FLAGS_scheduler_token);
  {
	std::scoped_lock lock1(tasks_mutex_);
	for (auto&& [k, task_desc] : tasks_) {
	  std::scoped_lock lock2(task_desc->mutex);

      // 是否完成或未启动
	  if (task_desc->state == TaskDesc::State::Pending ||
	      task_desc->state == TaskDesc::State::Done) {
	    continue;
	  }
      
	  // 是否已被中止
      if (task_desc->aborted.load(std::memory_order_relaxed)) {
		continue;
	  }

      // 长时间未keep-alive
	  if (now - task_desc->last_keep_alive_tp > 1min) {
		LOG_WARN("任务长时间未keep-alive，{}",task_desc->task_id);
		task_desc->aborted.store(true, std::memory_order_relaxed);
		continue;
	  }

	  // 添加
	  req.add_task_grant_ids(task_desc->task_grant_id);
	  task_ids.push_back(task_desc->task_id);
	}
  }
  req.set_next_keep_alive_in_ms(10s / 1ms);

  // 不需要keep alive
  if (req.task_grant_ids().empty()) {
	return;
  }

  // rpc
  SetTimeout(&context, 5s);
  auto status = scheduler_stub_->KeepTaskAlive(&context, req, &resp);
  if (!status.ok() || resp.statues_size() != req.task_grant_ids_size()) {
	LOG_WARN("RPC调用KeepTaskAlive失败：{}", status.error_message());
  } else {
    std::scoped_lock lock1(tasks_mutex_);
	for (std::size_t i = 0; i < task_ids.size(); ++i) {
	  if (resp.statues(i)) {
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

void TaskDispatcher::OnTimerKilledAbort(Poco::Timer& timer) {
  std::size_t aborted = 0;

  {
	std::scoped_lock lock(tasks_mutex_);
	for (auto&& [_, task_desc] : tasks_) {
	  if (!task_desc->aborted.load(std::memory_order_relaxed) &&
	      !IsProcAlive(task_desc->task->GetRequesterPid())) { // 没有aborted但程序却不存在
		task_desc->aborted.store(true, std::memory_order_relaxed);
		++aborted;
	  }
	}
  }

  if (aborted) {
	LOG_WARN("停止了{}个任务", aborted);
  }
}

void TaskDispatcher::OnTimerClear(Poco::Timer& timer) {
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
		LOG_DEBUG("任务已经完成，local task id = {}", iter->second->task_id);
		destroying.push_back(std::move(iter->second));
		iter = tasks_.erase(iter);
		continue;
	  }

	  // 超时未启动
	  if (iter->second->start_deadline + 1min < now) {
		LOG_WARN("任务太长时间未启动, local task id = {}", iter->second->task_id);
        destroying.push_back(std::move(iter->second));
		iter = tasks_.erase(iter);
		continue;
	  }

	  ++iter;
	}
  }

  // 析构自动释放
}

} // namespace distribuild::daemon::local