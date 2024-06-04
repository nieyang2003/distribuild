#pragma once

#include <shared_mutex>

#include "distribuild/common/token_verifier.h"

#include "daemon.grpc.pb.h"
#include "daemon.pb.h"

namespace distribuild::daemon::cloud {

class DaemonServiceImpl : public DaemonService::Service {
 public:
  explicit DaemonServiceImpl(std::string address);
  void Stop();
  void Join();

 public:
  /// @brief rpc函数，提交编译任务
  grpc::Status QueueCxxTask(grpc::ServerContext* context,
      const QueueCxxTaskRequest* request, QueueCxxTaskResponse* response) override;

  /// @brief rpc函数，添加任务的引用计数
  grpc::Status AddTaskRef(grpc::ServerContext* context,
      const AddTaskRefRequest* request, AddTaskRefResponse* response) override;

  /// @brief rpc函数，释放任务
  grpc::Status FreeTask(grpc::ServerContext* context,
      const FreeTaskRequest* request, FreeTaskResponse* response) override;

  /// @brief rpc函数，等待任务完成
  grpc::Status WaitForTask(grpc::ServerContext* context,
      const WaitForTaskRequest* request, WaitForTaskResponse* response) override;

 private:
  /// @brief 确认token是否有效
  bool IsTokenAcceptable(const std::string& token);

  /// @brief 更新tokens
  /// @param tokens 
  void UpdateTokens(std::unordered_set<std::string> tokens);

  /// @brief Timer函数，向scheduler发送心跳，设置自己的有关信息
  /// @param expire 下次心跳的超时时间
  void OnTimerHeartbeat(std::chrono::nanoseconds expire);
  
 private:
  std::string location_;
  std::shared_mutex token_mutex_;
  std::unique_ptr<TokenVerifier> token_verifier_ = std::make_unique<TokenVerifier>();
};

} // namespace distribuild::daemon::cloud