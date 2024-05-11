#pragma once

#include <vector>
#include <string>
#include <deque>
#include <mutex>
#include <chrono>
#include <memory>

#include "common/token_verifier.h"

#include "scheduler.grpc.pb.h"
#include "scheduler.pb.h"

namespace distribuild::scheduler {

class SchedulerServiceImpl : public SchedulerService::Service {
 public:
  SchedulerServiceImpl();

  // grpc方法
  grpc::Status HeartBeat(grpc::ServerContext* context, const HeartBeatRequest* request, HeartBeatResponse* response) override;
  grpc::Status GetConfig(grpc::ServerContext* context, const GetConfigRequest* request, GetConfigResponse* response) override;
  grpc::Status WaitForStaringTask(grpc::ServerContext* context, const WaitForStaringTaskRequest* request, WaitForStaringTaskReponse* response) override;
  grpc::Status KeepTaskAlive(grpc::ServerContext* context, const KeepTaskAliveRequest* request, KeepTaskAliveResponse* response) override;
  grpc::Status FreeTask(grpc::ServerContext* context, const FreeTaskRequst* request, FreeTaskResponse* response) override;
  grpc::Status GetRunningTasks(grpc::ServerContext* context, const GetRunningTasksRequest* request, GetRunningTasksResponse* response) override;

 private:
  std::vector<std::string> ActiveDaemonTokens();

 private:
  std::unique_ptr<TokenVerifier> user_token_verifier_;
  std::unique_ptr<TokenVerifier> servant_token_verifier_;

  std::mutex mutex_;
  std::chrono::steady_clock::time_point next_token_rollout_ = {};

  std::deque<std::string> active_daemon_tokens_;
};

} // namespace distribuild::scheduler