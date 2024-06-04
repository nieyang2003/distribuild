#pragma once

#include "distribuild/daemon/local/dist_task.h"

#include <optional>

#include "env_desc.grpc.pb.h"
#include "env_desc.pb.h"
#include "http_service.grpc.pb.h"
#include "http_service.pb.h"

namespace distribuild::daemon::local {

class CxxDistTask : public DistTask {
 public:
  using Output = std::pair<http_service::WaitForCXXTaskResponse, std::vector<std::string>>;

 public:
  pid_t GetRequesterPid() const override { return requester_pid_; }
  bool CacheControl() override  { return cache_control_; }
  const distribuild::EnviromentDesc& EnviromentDesc() const override {
	return env_desc_;
  }
  std::string CacheKey()  const override;
  std::string GetDigest() const override;
  void OnCompleted(const DistOutput& output) override {
	output_ = *RebuildOutput(std::move(output));
  }

  /// @brief 调用cloud rpc服务放入任务
  /// @param stub 
  /// @param token 
  /// @param grant_id 
  /// @return 
  std::optional<std::uint64_t>
  StartTask(cloud::DaemonService::Stub* stub, const std::string& token, std::uint64_t grant_id) override;

 public:
  grpc::Status Prepare(const http_service::SubmitCxxTaskRequest& req, const std::vector<std::string_view>& bytes);
  std::optional<Output> GetOutput() const { return output_; }
  std::optional<Output> RebuildOutput(const DistOutput& output);

 private:
  bool cache_control_;
  pid_t requester_pid_;
  distribuild::EnviromentDesc env_desc_;
  std::string source_path_;
  std::string source_digest_;
  std::string args_;
  std::string source_;

  Output output_;
};

} // namespace distribuild::daemon::local