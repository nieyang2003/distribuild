#include "common/spdlogging.h"
#include "common/token_verifier.h"
#include "scheduler/scheduler_service_impl.h"
#include "scheduler/task_dispatcher.h"
#include "common/encode.h"
#include <openssl/rand.h>
#include <chrono>
#include <gflags/gflags.h>
#include <optional>
#include <string_view>
#include <arpa/inet.h>

using namespace std::chrono_literals;

DEFINE_uint32(min_daemon_version, 0, "只接收比此版本号小的守护进程的请求");
DEFINE_int32(token_rollout_interval_s, 3000, "临牌轮换时间间隔，秒");
DEFINE_string(default_user_tokens, "nieyang", "");
DEFINE_string(default_servant_tokens, "nieyang", "");

namespace distribuild::scheduler {

namespace {

/// @brief 随机生成一个16位token
/// @return 
std::string NextDaemonToken() {
  char buf[16];
  DISTBU_CHECK(RAND_bytes(reinterpret_cast<unsigned char*>(buf), sizeof(buf)) == 1);
  return EncodeHex(std::string_view(buf, 16));
}

/// @brief 是否时ipv4地址
/// @param location 
/// @return 
std::optional<std::pair<std::string, std::string>> TryParseIPv4(const std::string& location) {
  auto pos = location.find(':');
  if (pos == std::string::npos) {
	return std::nullopt;
  }
  std::string ip = location.substr(0, pos);
  std::string port = location.substr(pos + 1);

  sockaddr_in addr_in{};
  if (inet_pton(AF_INET, ip.c_str(), &addr_in) != 1) {
	return std::nullopt;
  }

  pos = port.find_last_not_of("0123456789");
  if (pos != std::string::npos) {
	return std::nullopt;
  }

  return {{ip, port}};
}

std::optional<std::pair<std::string, std::string>> TryParseGrpcIPv4(const std::string& location) {
  constexpr std::string_view kPrefix = "ipv4:";
  if (location.compare(0, kPrefix.size(), kPrefix) != 0) {
    return std::nullopt;
  }
  return TryParseIPv4(location.substr(kPrefix.size()));
}

} // namespace

SchedulerServiceImpl::SchedulerServiceImpl() {
  active_daemon_tokens_ = {NextDaemonToken(), NextDaemonToken(), NextDaemonToken()};
  user_token_verifier_ = MakeTokenVerifier(FLAGS_default_user_tokens);
  servant_token_verifier_ = MakeTokenVerifier(FLAGS_default_servant_tokens);
  next_token_rollout_ = std::chrono::steady_clock::now() + FLAGS_token_rollout_interval_s * 1s;
}

grpc::Status SchedulerServiceImpl::HeartBeat(grpc::ServerContext* context, const HeartBeatRequest* request, HeartBeatResponse* response) {
  // 验证token
  if (!user_token_verifier_->Verify(request->token()) &&
      !servant_token_verifier_->Verify(request->token())) {
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }

  // 验证版本
  if (request->version() < FLAGS_min_daemon_version) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "版本号大于自己");
  }

  // 验证超时
  if (request->next_heart_beat_in_ms() * 1ms > 15s) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "不合理的超时时间");
  }

  std::string observed_location, reported_location;

  // 验证IP
  auto reported_ip_port_pair = TryParseIPv4(request->location());
  if (!reported_ip_port_pair) {
	LOG_ERROR("非法地址 `{}`", request->location());
	return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "非法地址");
  } else {
	auto grpc_peer = context->peer();
	auto observed_ip_port_pair = TryParseGrpcIPv4(context->peer());
	if (!observed_ip_port_pair) {
      LOG_WARN("暂不支持的地址 `{}`", context->peer());
	  return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "暂不支持的地址");
	}
	reported_location = request->location(); // 请求中自己写的
	observed_location = observed_ip_port_pair->first + ":" + reported_ip_port_pair->second;   // 实际地址与端口
  }

  // 设置节点信息
  ServantInfo servant_info;
  servant_info.version = request->version();
  servant_info.observed_location = observed_location;
  servant_info.reported_location = reported_location;
  servant_info.num_cpu_cores = request->num_cpu_cores();
  servant_info.current_load = request->current_load();
  if (servant_info.num_cpu_cores == 0) {
    LOG_WARN("`{}`未报告cpu核心数", context->peer());
	servant_info.num_cpu_cores = request->concurrency();
  }
  servant_info.total_memory_in_bytes = request->total_memory_in_bytes();
  servant_info.avail_memory_in_bytes = request->avail_memory_in_bytes();
  servant_info.concurrency = request->concurrency();
  servant_info.priority = request->priority();
  if (servant_info.priority == ServantPriority::SERVANT_PRIORITY_UNKNOWN) {
	servant_info.priority = ServantPriority::SERVANT_PRIORITY_USER;
  }
  if (observed_location != reported_location) {
	// NAT无法主动发送数据
	LOG_TRACE("NAT节点, reported_location = {}, observed_location = {}", reported_location, observed_location);
    servant_info.concurrency = 0;
  }
  if (!servant_token_verifier_->Verify(request->token())) {
	// user
	servant_info.concurrency = 0;
  }
  if (request->next_heart_beat_in_ms() == 0) {
	// 心跳时间为0应该被移除
    servant_info.concurrency = 0;
  }
  for (auto&& env : request->env_descs()) {
    servant_info.env_decs.push_back(env);
  }

  // 新增或更新节点
  TaskDispatcher::Instance()->KeepServantAlive(servant_info, request->next_heart_beat_in_ms() * 1ms);

  // 返回3个token
  for (auto&& token : ActiveDaemonTokens()) {
	response->add_tokens(token);
  }

  // 返回应该在其上运行的任务
  auto expired_task = TaskDispatcher::Instance()->NotifyServantRunningTasks(request->location(), {request->running_tasks().begin(), request->running_tasks().end()});
  for (auto&& e : expired_task) {
	response->add_expired_tasks(e);
  }

  return grpc::Status::OK;
}

grpc::Status SchedulerServiceImpl::GetConfig(grpc::ServerContext* context, const GetConfigRequest* request, GetConfigResponse* response) {
  LOG_INFO("RPC调用者：{}", context->peer());

  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }
  response->set_daemon_token(ActiveDaemonTokens()[1]); // 正在使用的

  return grpc::Status::OK;
}

grpc::Status SchedulerServiceImpl::WaitForStaringTask(grpc::ServerContext* context, const WaitForStaringTaskRequest* request, WaitForStaringTaskReponse* response) {
  LOG_INFO("RPC调用者：{}", context->peer());

  // 验证Token
  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }

  auto max_wait = request->mills_to_wait() * 1ms;
  auto next_keep_alive = request->next_keep_alive_in_ms() * 1ms;
  if (max_wait > 15s || next_keep_alive > 30s) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "不合理的超时时间");
  }

  TaskInfo task {
	.requester_ip = context->peer(),
	.min_version  = request->min_version(),
	.env_desc     = request->env_desc(),
  };
  auto now = std::chrono::steady_clock::now();

  // 处理立即任务
  for (uint32_t i = 0; i != request->immeadiate_reqs(); ++i) {
	auto result = TaskDispatcher::Instance()->WaitForStartingNewTask(task, next_keep_alive, now + (i == 0 ? max_wait : 0s), false);
	if (!result.first) {
	  if (result.second == WaitStatus::EnvNotFound) {
	  	return grpc::Status(grpc::StatusCode::NOT_FOUND, "未找到对应编译器");
	  }
	  break;
	}

	auto&& added = response->add_grants();
	added->set_task_grant_id(result.first->task_id);
	added->set_allocated_servant_location(new std::string(result.first->servant_location));
  }

  // 处理预取任务
  for (uint32_t i = 0; i != request->prefetch_reqs(); ++i) {
	auto result = TaskDispatcher::Instance()->WaitForStartingNewTask(task, next_keep_alive, now + (response->grants().empty() ? max_wait : 0s), true);
	if (!result.first) {
	  break;
	}

	auto added = response->add_grants();
	added->set_task_grant_id(result.first->task_id);
	added->set_allocated_servant_location(new std::string(result.first->servant_location));
  }

  if (response->grants().empty()) {
	return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "无编译资源");
  }

  return grpc::Status::OK;
}

grpc::Status SchedulerServiceImpl::KeepTaskAlive(
    grpc::ServerContext* context, const KeepTaskAliveRequest* request,
    KeepTaskAliveResponse* response) {
  LOG_INFO("RPC调用者：{}", context->peer());

  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }

  auto next_keep_alive = request->next_keep_alive_in_ms() * 1ms;
  if (next_keep_alive > 30s) {
	return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "时间过长");
  }

  for (auto&& task_id : request->task_grant_ids()) {
	response->add_statues(TaskDispatcher::Instance()->KeepTaskAlive(task_id, next_keep_alive));
  }

  return grpc::Status::OK;
}

grpc::Status SchedulerServiceImpl::FreeTask(grpc::ServerContext *context, const FreeTaskRequst *request, FreeTaskResponse *response) {
  LOG_INFO("RPC调用者：{}", context->peer());

  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }
  for (auto&& task_id : request->task_grant_ids()) {
    TaskDispatcher::Instance()->FreeTask(task_id);
  }
  return grpc::Status::OK;
}

grpc::Status SchedulerServiceImpl::GetRunningTasks(grpc::ServerContext *context, const GetRunningTasksRequest *request, GetRunningTasksResponse *response) {
  for (auto&& running_task : TaskDispatcher::Instance()->GetRunningTasks()) {
    *response->add_running_tasks() = std::move(running_task);
  }
  return grpc::Status::OK;
}

std::vector<std::string> SchedulerServiceImpl::ActiveDaemonTokens() {
  std::scoped_lock _(mutex_);
  auto now = std::chrono::steady_clock::now();

  if (next_token_rollout_ < now) {
	// 临牌轮换
	next_token_rollout_ = now + FLAGS_token_rollout_interval_s * 1s;
	active_daemon_tokens_.pop_front();
	active_daemon_tokens_.push_back(NextDaemonToken());
  }

  DISTBU_CHECK(active_daemon_tokens_.size() == 3);
  return std::vector<std::string>(active_daemon_tokens_.begin(), active_daemon_tokens_.end());
}

} // namespace distribuild::scheduler