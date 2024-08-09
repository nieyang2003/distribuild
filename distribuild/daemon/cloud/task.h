#pragma once

#include <memory>
#include <string>
#include <optional>

#include "google/protobuf/any.pb.h"

namespace distribuild::daemon::cloud {

/// @brief 任务基类
class Task : public std::enable_shared_from_this<Task> {
 public:
  virtual ~Task() {};
  
  // 返回要执行的命令
  virtual std::string GetCmdLine() const = 0;
  // 获取输入，转移了所有权
  virtual std::string GetSource() = 0;
  // 编译完成
  virtual void OnCompleted(int exit_code, std::string&& std_out, std::string&& std_err) = 0;
};

/// @brief 编译任务基类
class CompileTask : public Task {
 protected:
  struct Output {
    std::vector<std::pair<std::string, std::string>> files; // 文件扩展名-内容
	google::protobuf::Any extra_info;
  };
 public:
  virtual ~CompileTask() override = default;
  virtual std::optional<std::string> GetCacheKey() const = 0;
  virtual std::string GetDigest() const = 0;
  virtual int GetExitCode() const = 0;
  virtual const std::string& GetStdout() const = 0;
  virtual const std::string& GetStderr() const = 0;
  virtual std::optional<Output> GetOutput(int exit_code, std::string& std_out, std::string& std_err) = 0;
  const std::string& GetFilePack() const { return file_pack_; }
  const google::protobuf::Any& GetExtraInfo() const { return extra_info_; }
 protected:
  std::string file_pack_;
  google::protobuf::Any extra_info_;
};

}