#include "daemon_service_impl.h"

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>

#include "scheduler.grpc.pb.h"
#include "scheduler.pb.h"

#include "distribuild/common/logging.h"
#include "distribuild/daemon/version.h"
#include "distribuild/daemon/cloud/config.h"
#include "distribuild/daemon/cloud/executor.h"
#include "distribuild/daemon/cloud/compilers.h"
#include "distribuild/daemon/cloud/compile_task/cxx_task.h"

using namespace std::literals;

namespace distribuild::daemon::cloud {

namespace {

/// @brief 读取grpc server context中的附加文件
/// @return 
std::optional<std::string> GetAttachment(grpc::ServerContext* context) {
  auto it = context->client_metadata().find("attachment");
  if (it != context->client_metadata().end()) {
      return std::string(it->second.data(), it->second.length());
  } else {
      // 没有找到附件，返回空的 std::optional
      return std::nullopt;
  }
}

void SetAttachment(grpc::ServerContext* context, const std::string& data) {
  context->AddInitialMetadata("attachment", data);
}

}

bool DaemonServiceImpl::IsTokenAcceptable(const std::string& token) {
  std::shared_lock lock(token_mutex_);
  return token_verifier_->Verify(token);
}

void DaemonServiceImpl::UpdateTokens(std::unordered_set<std::string> tokens) {
  std::scoped_lock lock(token_mutex_);
  token_verifier_ = std::make_unique<TokenVerifier>(std::move(tokens));
}

void DaemonServiceImpl::OnTimerHeartbeat(std::chrono::nanoseconds expire) {
  // 连接
  grpc::ClientContext context;
  auto channel = grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials());
  auto stub = scheduler::SchedulerService::NewStub(channel);
  if (stub == nullptr) {
	LOG_WARN("发起心跳连接失败");
	return;
  }

  // 造包
  scheduler::HeartBeatRequest req;
  req.set_token("nieyang2003"); // TODO:
  req.set_next_heart_beat_in_ms(expire / 1ms);
  req.set_version(distribuild::DISTRIBUILD_VERSION);
  req.set_reported_location(location_);
  req.set_servant_priority(scheduler::SERVANT_PRIORITY_USER); // TODO:
  req.set_memory_available_in_bytes(config::GetMemoryAvailable());
  req.set_total_memory_in_bytes(config::GetMemoryTotal()); // TODO:
  req.set_concurrency(std::thread::hardware_concurrency());
  req.set_current_load(0); // TODO
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
	auto compile_task = dynamic_cast<CompileTask*>(e.task.get());
	DISTBU_CHECK(compile_task);
	running_task->set_task_digest(compile_task);
  }

  // 发送
  scheduler::HeartBeatResponse res;
  auto status = stub->HeartBeat(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("发送心跳失败: {}", status.error_details());
  }

  // 更新状态
  Executor::Instance()->KillExpiredTasks({res.expired_tasks().begin(), res.expired_tasks().end()});
  UpdateTokens({res.tokens().begin(), res.tokens().end()});
}

DaemonServiceImpl::DaemonServiceImpl(std::string location)
  : location_(location) {
  // TODO: 设置定时任务心跳
}

void DaemonServiceImpl::Stop() {
  // TODO: 停止定时器
}

void DaemonServiceImpl::Join() {}

grpc::Status DaemonServiceImpl::QueueCxxTask(grpc::ServerContext* context,
                                             const QueueCxxTaskRequest* request,
                                             QueueCxxTaskResponse* response) {
  // 验证Token
  if (!IsTokenAcceptable(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Invalid Token");
  }

  // 解析请求并处理到task中
  std::shared_ptr<CxxCompileTask> task = std::make_shared<CxxCompileTask>();
  if (auto status = task->Prepare(*request, *GetAttachment(context)); !status.ok()) {
    return status;
  }

  // 提交编译得到本机task_id
  auto task_id = Executor::Instance()->TryQueueTask(request->task_grant_id(), task);
  if (!task_id) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Too many tasks is running");
  }

  // 将任务task_id返回
  response->set_task_id(*task_id);
  return grpc::Status::OK;
}

grpc::Status DaemonServiceImpl::AddTaskRef(grpc::ServerContext* context,
                                           const AddTaskRefRequest* request,
                                           AddTaskRefResponse* response) {
  // 验证Token
  if (!IsTokenAcceptable(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "invalid token");
  }

  // 添加计数
  if (!Executor::Instance()->TryAddTaskRef(request->task_id())) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "task not found");
  }

  return grpc::Status::OK;
}

grpc::Status DaemonServiceImpl::FreeTask(grpc::ServerContext* context,
                                         const FreeTaskRequest* request,
                                         FreeTaskResponse* response) {
  // 验证Token
  if (!IsTokenAcceptable(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "invalid token");
  }

  // 释放
  Executor::Instance()->FreeTask(request->task_id());

  return grpc::Status::OK;
}

grpc::Status DaemonServiceImpl::WaitForTask(grpc::ServerContext* context,
                                            const WaitForTaskRequest* request,
                                            WaitForTaskResponse* response) {
  // 验证Token
  if (!IsTokenAcceptable(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "invalid token");
  }

  // 验证压缩类型
  if (request->acceptable_compress_types().at(0) != CompressType::COMPRESS_TYPE_ZSTD) {
	return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid compress type");
  }

  // 等待任务
  auto output = Executor::Instance()->WaitForTask(request->task_id(), request->wait_ms() * 1ms);
  if (!output.first) {
	if (output.second == ExecutionStatus::Failed) {
	  response->set_task_status(TaskStatus::TASK_STATUS_FAILED);
	} else if (output.second == ExecutionStatus::Running) {
	  response->set_task_status(TaskStatus::TASK_STATUS_RUNNING);
	} else if (output.second == ExecutionStatus::NotFound) {
      response->set_task_status(TaskStatus::TASK_STATUS_NOT_FOUND);
	} else {
	  DISTBU_CHECK(false, "`WaitForTask()`内部错误");
	}
	return;
  }

  // 获得输出
  auto task = dynamic_cast<CompileTask*>(output.first.get());
  response->set_task_status(TaskStatus::TASK_STATUS_DONE);
  response->set_exit_code(task->GetExitCode());
  response->set_output(task->GetStdout());
  response->set_err(task->GetStderr());
  response->set_compress_type(CompressType::COMPRESS_TYPE_ZSTD);
  *response->mutable_extra_info() = task->GetExtraInfo();
  SetAttachment(context, task->GetFilePack());

  return grpc::Status::OK;
}

}  // namespace distribuild::daemon::cloud