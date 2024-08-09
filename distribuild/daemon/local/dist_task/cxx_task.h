#pragma once
#include <optional>
#include "daemon/local/dist_task.h"
#include "../build/distribuild/proto/env_desc.grpc.pb.h"
#include "../build/distribuild/proto/env_desc.pb.h"
#include "../build/distribuild/proto/http_service.grpc.pb.h"
#include "../build/distribuild/proto/http_service.pb.h"

namespace distribuild::daemon::local {

class CxxDistTask : public DistTask {
 public:
  using Output = std::pair<http_service::WaitForCXXTaskResponse, std::vector<std::string>>;

 public:
  CxxDistTask() {}
  virtual ~CxxDistTask() override	 = default;

  pid_t GetRequesterPid() const override { return requester_pid_; }
  bool CacheControl() override  { return cache_control_; }
  const distribuild::EnviromentDesc& GetEnviromentDesc() const override { return env_desc_; }

  std::string CacheKey()  const override;
  std::string GetDigest() const override;

  /// @brief 任务完成，写入结果，使用移动
  void OnCompleted(DistOutput&& output) override { output_ = std::move(*RebuildOutput(std::move(output))); }

  /// @brief 通知cloud开始编译
  /// @return 
  std::optional<std::uint64_t>
  StartTask(cloud::DaemonService::Stub* stub, const std::string& token, std::uint64_t grant_id) override;

  grpc::Status Prepare(const http_service::SubmitCxxTaskRequest& req, const std::vector<std::string_view>& bytes);
  std::optional<Output> GetOutput() const { return output_; }

  std::optional<Output> RebuildOutput(DistOutput&& output);

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