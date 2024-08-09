#pragma once

#include <string>
#include <vector>
#include <optional>
#include "google/protobuf/any.pb.h"
#include "../build/distribuild/proto/env_desc.grpc.pb.h"
#include "../build/distribuild/proto/daemon.grpc.pb.h"

namespace distribuild::daemon::local {

struct DistTask {
 public:
  virtual ~DistTask() = default;
  
  struct DistOutput {
    int exit_code;
	std::string std_out;
	std::string std_err;
	google::protobuf::Any extra_info; // ? 是否支持移动语义?
	std::vector<std::pair<std::string, std::string>> output_files;
  };
  
  virtual bool CacheControl() = 0;
  virtual std::string CacheKey() const = 0;
  virtual std::string GetDigest() const = 0;
  virtual pid_t GetRequesterPid() const = 0;
  virtual const EnviromentDesc& GetEnviromentDesc() const = 0;

  virtual void OnCompleted(DistOutput&& output) = 0;

  virtual std::optional<std::uint64_t>
  StartTask(cloud::DaemonService::Stub* stub, const std::string& token, std::uint64_t grant_id) = 0;
};

}