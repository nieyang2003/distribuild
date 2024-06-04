#pragma once

#include <future>

#include "distribuild/daemon/cloud/task.h"
#include "distribuild/daemon/cloud/temp_dir.h"

#include "daemon.grpc.pb.h"
#include "daemon.pb.h"
#include "env_desc.grpc.pb.h"
#include "env_desc.pb.h"

namespace distribuild::daemon::cloud {

class CxxCompileTask : public CompileTask {
 public:
  CxxCompileTask();

  void OnCompleted(int exit_code, std::string std_out, std::string std_err) override;
  std::optional<std::string> GetCacheKey() const override;
  std::string GetDigest() const override;
  std::optional<Output> GetOutput(int exit_code, std::string& std_out, std::string& std_err) override;

  std::string GetStdInput() override { return std::move(stdin_); }
  std::string GetCmdLine() const override { return cmdline_; }
  int GetExitCode() const override { return exit_code_; }
  const std::string& GetStdout() const override { return stdout_; }
  const std::string& GetStderr() const override { return stderr_; }

  grpc::Status Prepare(const QueueCxxTaskRequest& request, const std::string& attachment);

 private:
  // 结果
  int exit_code_;
  std::string stdin_;
  std::string stdout_;
  std::string stderr_;

  // 缓存
  bool write_cache_;
  std::future<bool> write_cache_future_;

  // 源码信息
  EnviromentDesc env_desc_;
  std::string cmdline_;
  std::string args_;
  std::string source_;        // 源代码
  std::string source_path_;
  std::string source_digest_;

  // 工作路径
  TempDir work_dir_;
  std::string temp_sub_dir_;
};

} // namespace distribuild::daemon::cloud