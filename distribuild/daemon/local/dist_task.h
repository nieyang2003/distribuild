#pragma once

#include <string>
#include <vector>
#include <optional>

#include "env_desc.grpc.pb.h"
#include "daemon.grpc.pb.h"

namespace distribuild::daemon::local {

struct DistTask {
 public:
  virtual ~DistTask() = default;
  
  struct DistOutput {
    int exit_code;
	std::string std_out;
	std::string std_err;
	std::vector<std::pair<std::string, std::string>> output_files;
  };
  
  virtual bool CacheControl() = 0;
  virtual std::string CacheKey() const = 0;
  virtual std::string GetDigest() const = 0;
  virtual pid_t GetRequesterPid() const = 0;
//   virtual std::optional<std::string> GetOutput() const = 0;/
  virtual const EnviromentDesc& EnviromentDesc() const = 0;
  virtual void OnCompleted(const DistOutput& output) = 0;

  virtual std::optional<std::uint64_t>
  StartTask(cloud::DaemonService::Stub* stub, const std::string& token, std::uint64_t grant_id) = 0;
};

}