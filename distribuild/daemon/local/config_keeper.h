#pragma once

#include <string>
#include <mutex>
#include <memory>

#include "scheduler.grpc.pb.h"
#include "scheduler.pb.h"

namespace distribuild::daemon::local {

class ConfigKeeper {
 public:
  ConfigKeeper();

  std::string GetServingDaemonToken() const;

  void Stop();
  void Join();
 
 private:
  void OnTimerFetchConfig();

 private:
  mutable std::mutex token_mutex_;
  std::string serving_daemon_token_;

  std::unique_ptr<scheduler::SchedulerService::Stub> stub_;
};

} // namespace distribuild::daemon::local