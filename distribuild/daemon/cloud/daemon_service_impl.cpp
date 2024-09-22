#include "daemon_service_impl.h"
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include <gflags/gflags.h>
#include "../build/distribuild/proto/scheduler.grpc.pb.h"
#include "../build/distribuild/proto/scheduler.pb.h"
#include "common/spdlogging.h"
#include "daemon/version.h"
#include "daemon/config.h"
#include "daemon/cloud/executor.h"
#include "daemon/cloud/compilers.h"
#include "daemon/cloud/compile_task/cxx_task.h"
#include "daemon/sysinfo.h"

using namespace std::literals;

namespace distribuild::daemon::cloud {

bool DaemonServiceImpl::IsTokenAcceptable(const std::string& token) {
  std::shared_lock lock(token_mutex_);
  return token_verifier_->Verify(token);
}

void DaemonServiceImpl::UpdateTokens(std::unordered_set<std::string> tokens) {
  std::scoped_lock lock(token_mutex_);
  token_verifier_ = std::make_unique<TokenVerifier>(std::move(tokens));
}

void DaemonServiceImpl::OnTimerHeartbeat(Poco::Timer& timer) {
  static const std::unordered_map<std::string, scheduler::ServantPriority> kServantPriorityMap = {
    {"user", scheduler::ServantPriority::SERVANT_PRIORITY_USER},
	{"dedicated", scheduler::ServantPriority::SERVANT_PRIORITY_DEDICATED},
  };

  // 连接
  grpc::ClientContext context;
  SetTimeout(&context, 2s);
  auto channel = grpc::CreateChannel(FLAGS_scheduler_location, grpc::InsecureChannelCredentials());
  auto stub = scheduler::SchedulerService::NewStub(channel);
  if (stub == nullptr) {
	LOG_WARN("创建grpc存根失败");
	return;
  }

  // 造包
  scheduler::HeartBeatRequest req;
  req.set_token(FLAGS_scheduler_token);
  req.set_next_heart_beat_in_ms(10s / 1ms);
  req.set_version(distribuild::DISTRIBUILD_VERSION);
  req.set_location(location_);
  req.set_priority(kServantPriorityMap.at(FLAGS_servant_priority));
  req.set_avail_memory_in_bytes(GetAvailMemory());
  req.set_total_memory_in_bytes(GetTotalMemory());
  // 设置CPU限制
  req.set_concurrency(Executor::Instance()->GetMaxConcurrency());
  req.set_num_cpu_cores(GetNumCPUCores());
  req.set_current_load(GetCurrentLoad(FLAGS_cpu_load_average_seconds * 1s));
  // 设置编译器
  for (auto&& e : Compilers::Instance()->GetAll()) {
	*req.add_env_descs() = e;
  }
  // 设置正在允许的任务数量
  for (auto&& e : Executor::Instance()->GetAllTasks()) {
	auto running_task = req.add_running_tasks();
	running_task->set_servant_location(location_);
	running_task->set_task_grant_id(e.task_grant_id);
	running_task->set_servant_task_id(e.servant_task_id);
	running_task->set_task_digest(static_cast<CompileTask*>(e.task.get())->GetDigest());
  }

  // 发送
  scheduler::HeartBeatResponse resp;
  auto status = stub->HeartBeat(&context, req, &resp);
  if (!status.ok()) {
	LOG_WARN("心跳失败: {}", status.error_message());
	return;
  }

  // 更新状态
  Executor::Instance()->KillExpiredTasks({resp.expired_tasks().begin(), resp.expired_tasks().end()});
  UpdateTokens({resp.tokens().begin(), resp.tokens().end()});
}

DaemonServiceImpl::DaemonServiceImpl(std::string location)
  : timer_(0, FLAGS_heart_beat_timer_intervals)
  , location_(location) {
  LOG_INFO("调度器地址：'{}'", location);
  LOG_DEBUG("启动定时器 OnTimerHeartbeat");
  timer_.start(Poco::TimerCallback<DaemonServiceImpl>(*this, &DaemonServiceImpl::OnTimerHeartbeat));
}

void DaemonServiceImpl::Stop() {
  timer_.stop();
}

void DaemonServiceImpl::Join() {}

grpc::Status DaemonServiceImpl::QueueCxxTask(grpc::ServerContext *context, grpc::ServerReader<QueueCxxTaskRequestChunk> *reader, QueueCxxTaskResponse *response) {
  LOG_INFO("收到一个任务请求，来自'{}'", context->peer());

  QueueCxxTaskRequestChunk chunk;
  std::unique_ptr<QueueCxxTaskRequest> request;
  std::string file;
  bool is_first_chunk = true;

  while (reader->Read(&chunk)) {
    if (is_first_chunk) [[unlikely]] {
	  // 处理第一个块，请求
	  if (!chunk.has_request() || chunk.file_chunk().size() != 0) [[unlikely]] {
		return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "rpc格式错误");
	  }

	  request.reset(chunk.release_request());
	  if (!IsTokenAcceptable(request->token())) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
      }

	  is_first_chunk = false;
	} else [[likely]] {
	  // 处理后续块，文件
      if (chunk.has_request()) [[unlikely]] {
		return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "rpc格式错误");
	  }

	  file.append(chunk.file_chunk().data(), chunk.file_chunk().size());
	}
  }
  LOG_DEBUG("文件大小：{}", file.size());

  // 解析请求并处理到task中
  std::shared_ptr<CxxCompileTask> task = std::make_shared<CxxCompileTask>();
  if (auto status = task->Prepare(*request, file); !status.ok()) {
	LOG_INFO("Prepare失败：{}", status.error_message());
    return status;
  }

  // 提交编译得到本机task_id
  auto task_id = Executor::Instance()->TryQueueTask(request->task_grant_id(), task);
  if (!task_id) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "当前任务太多");
  }

  // 将任务task_id返回
  response->set_task_id(*task_id);
  return grpc::Status::OK;
}

grpc::Status DaemonServiceImpl::AddTaskRef(grpc::ServerContext *context, const AddTaskRefRequest *request, AddTaskRefResponse *response) {
  LOG_DEBUG("调用者：'{}'", context->peer());

  // 验证Token
  if (!IsTokenAcceptable(request->token())) {
  	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }

  // 添加计数
  if (!Executor::Instance()->TryAddTaskRef(request->task_id())) {
  	return grpc::Status(grpc::StatusCode::NOT_FOUND, "未找到任务");
  }

  return grpc::Status::OK;
}

grpc::Status DaemonServiceImpl::FreeTask(grpc::ServerContext* context, const FreeTaskRequest* request, FreeTaskResponse* response) {
  LOG_INFO("调用者：'{}'", context->peer());

  // 验证Token
  if (!IsTokenAcceptable(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }

  // 释放
  Executor::Instance()->FreeTask(request->task_id());

  return grpc::Status::OK;
}

grpc::Status DaemonServiceImpl::WaitForTask(grpc::ServerContext *context, const WaitForTaskRequest *request, grpc::ServerWriter<WaitForTaskResponseChunk>* writer) {
  LOG_INFO("调用者：'{}'", context->peer());

  // 验证Token
  if (!IsTokenAcceptable(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }

  // 验证压缩类型
  if (request->acceptable_compress_types().at(0) != CompressType::COMPRESS_TYPE_ZSTD) {
  	return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "非法压缩类型");
  }

  // 等待任务
  WaitForTaskResponse* response = new WaitForTaskResponse();
  auto output = Executor::Instance()->WaitForTask(request->task_id(), request->wait_ms() * 1ms);
  if (!output.first) {
  	if (output.second == ExecutionStatus::Failed) {
  	  response->set_task_status(TaskStatus::TASK_STATUS_FAILED);
  	} else if (output.second == ExecutionStatus::Running) {
  	  response->set_task_status(TaskStatus::TASK_STATUS_RUNNING);
  	} else if (output.second == ExecutionStatus::NotFound) {
  	  response->set_task_status(TaskStatus::TASK_STATUS_NOT_FOUND);
  	} else {
  	  DISTBU_CHECK_FORMAT(false, "`WaitForTask()`内部错误");
  	}
  	return grpc::Status(grpc::StatusCode::NOT_FOUND, "等待任务失败");
  }

  // 获得输出
  auto task = static_cast<CompileTask*>(output.first.get());
  response->set_task_status(TaskStatus::TASK_STATUS_DONE);
  response->set_exit_code(task->GetExitCode());
  response->set_output(task->GetStdout());
  response->set_err(task->GetStderr());
  response->set_compress_type(CompressType::COMPRESS_TYPE_ZSTD);
  *response->mutable_extra_info() = task->GetExtraInfo();

  // 第一个报文
  WaitForTaskResponseChunk first_chunk;
  first_chunk.set_allocated_response(response);
  writer->Write(first_chunk);

  // 发送文件
  auto&& file = task->GetFilePack();
  LOG_DEBUG("file size = {}", file.size());
  for (std::size_t i = 0; i < file.size(); i += FLAGS_chunk_size) {
	WaitForTaskResponseChunk chunk;
	size_t remaining_size = file.size() - i;
	chunk.set_file_chunk(file.data() + i, std::min(FLAGS_chunk_size, remaining_size));
    writer->Write(chunk);
  }

  return grpc::Status::OK;
}

}  // namespace distribuild::daemon::cloud