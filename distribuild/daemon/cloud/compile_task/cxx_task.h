#pragma once

#include <future>
#include "daemon/cloud/task.h"
#include "daemon/cloud/temp_dir.h"
#include "../build/distribuild/proto/daemon.grpc.pb.h"
#include "../build/distribuild/proto/daemon.pb.h"
#include "../build/distribuild/proto/env_desc.grpc.pb.h"
#include "../build/distribuild/proto/env_desc.pb.h"

namespace distribuild::daemon::cloud {

class CxxCompileTask : public CompileTask {
 public:
  CxxCompileTask();
  ~CxxCompileTask() override = default;

  /// @brief 编译完成，需要移动语义
  void OnCompleted(int exit_code, std::string&& std_out, std::string&& std_err) override;

  /// @brief 如果允许写入缓存则生成摘要作为key值
  std::optional<std::string> GetCacheKey() const override;

  /// @brief 由编译器摘要、编译参数、源码摘要生成新摘要
  std::string GetDigest() const override;

  /// @brief 获取编译结果文件
  std::optional<Output> GetOutput(int exit_code, std::string& std_out, std::string& std_err) override;

  /// @brief 获取源码，编译写入一次所以移动所有权
  std::string GetSource() override              { return std::move(source_); }

  std::string GetCmdLine() const override       { return cmdline_; }
  int GetExitCode() const override              { return exit_code_; }

  // 可能多次获得结果，不需要移动

  const std::string& GetStdout() const override { return stdout_; }
  const std::string& GetStderr() const override { return stderr_; }

  /// @brief 预处理请求，拿到file后立即解压了，用不用移动语义无所谓
  grpc::Status Prepare(const QueueCxxTaskRequest& request, const std::string& file);

 private:
  // 结果
  int exit_code_;
  std::string stdout_;
  std::string stderr_;

  // 缓存
  bool write_cache_ = false;
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