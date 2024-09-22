#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include "daemon/config.h"
#include <grpcpp/create_channel.h>
#include "daemon/local/config_keeper.h"
#include "common/spdlogging.h"

namespace distribuild::daemon::local {

ConfigKeeper::ConfigKeeper()
  : timer_(0, 10'000) /* 10s */ {
  auto channel = grpc::CreateChannel(FLAGS_scheduler_location, grpc::InsecureChannelCredentials());
  stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(stub_);

  LOG_INFO("启动定时器 OnTimerFetchConfig");
  timer_.start(Poco::TimerCallback<ConfigKeeper>(*this, &ConfigKeeper::OnTimerFetchConfig));
}

std::string ConfigKeeper::GetServingDaemonToken() const {
  std::scoped_lock lock(token_mutex_);
  return serving_daemon_token_;
}

void ConfigKeeper::Stop() {
  timer_.stop();
}

void ConfigKeeper::Join() {}

void ConfigKeeper::OnTimerFetchConfig(Poco::Timer& timer) {
  grpc::ClientContext context;
  scheduler::GetConfigRequest  req;
  scheduler::GetConfigResponse res;

  req.set_token(FLAGS_scheduler_token);
  auto status = stub_->GetConfig(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("获取配置失败");
	return;
  }
  LOG_INFO("获取配置成功，token = {}", res.daemon_token());

  std::scoped_lock lock(token_mutex_);
  serving_daemon_token_ = res.daemon_token();
}

}