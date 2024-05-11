#include "scheduler_service_impl.h"
#include "logging.h"
#include "openssl/rand.h"
#include "token_verifier.h"

#include <chrono>
using namespace std::chrono_literals;

namespace distribuild::scheduler {

namespace {

std::string NextDaemonToken() {
  char buf[16];
  DISTBU_CHECK(RAND_bytes(reinterpret_cast<unsigned char*>(buf), sizeof(buf)), 1);
  return std::string(buf);
}

} // namespace

SchedulerServiceImpl::SchedulerServiceImpl() {
  active_daemon_tokens_ = {NextDaemonToken(), NextDaemonToken(), NextDaemonToken()};
  user_token_verifier_ = MakeTokenVerifier(std::string());
  servant_token_verifier_ = MakeTokenVerifier(std::string());
  next_token_rollout_ = std::chrono::steady_clock::now();
}

grpc::Status SchedulerServiceImpl::HeartBeat(grpc::ServerContext* context, const HeartBeatRequest* request, HeartBeatResponse* response) {
  // 验证token
  if (!user_token_verifier_->Verify(request->token()) && !servant_token_verifier_->Verify(request->token())) {
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token Not Found.");
  }

  // 验证版本
  if (request->version() < 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Version too old.");
  }

  // 验证IP

  // 验证超时

  // 设置节点

  // NAT

  // response
}

grpc::Status SchedulerServiceImpl::GetConfig(grpc::ServerContext* context, const GetConfigRequest* request, GetConfigResponse* response) {
  // TODO: log

  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token Not Found.");
  }
  response->set_daemon_token(NextDaemonToken());
}

grpc::Status SchedulerServiceImpl::WaitForStaringTask(grpc::ServerContext* context, const WaitForStaringTaskRequest* request, WaitForStaringTaskReponse* response) {
  // TODO: log

  // 验证Token
  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token Not Found.");
  }

  // 

}

grpc::Status SchedulerServiceImpl::KeepTaskAlive(
    grpc::ServerContext* context, const KeepTaskAliveRequest* request,
    KeepTaskAliveResponse* response) {
  // TODO: log

  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token Not Found.");
  }

  auto next_keep_alive = request->next_keep_alive_in_ms() * 1ms;
  if (next_keep_alive > 30s) {
	return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Too Long");
  }

  for (auto&& e : request->task_grant_ids()) {
	// TODO
  }

  return grpc::Status::OK;
}

}  // namespace distribuild::scheduler