#pragma once

#include <string>
#include <mutex>
#include <memory>
#include <Poco/Timer.h>
#include "../build/distribuild/proto/scheduler.grpc.pb.h"
#include "../build/distribuild/proto/scheduler.pb.h"

namespace distribuild::daemon::local {

/// @brief 定时获得Token
class ConfigKeeper {
 public:
  ConfigKeeper();

  std::string GetServingDaemonToken() const;

  void Stop();
  void Join();
 
 private:
  void OnTimerFetchConfig(Poco::Timer& timer);

 private:
  Poco::Timer timer_;
  mutable std::mutex token_mutex_;
  std::string serving_daemon_token_;
  std::unique_ptr<scheduler::SchedulerService::Stub> stub_;
};

} // namespace distribuild::daemon::local