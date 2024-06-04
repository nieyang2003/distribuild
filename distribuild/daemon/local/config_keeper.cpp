#include "config_keeper.h"

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>

#include "distribuild/common/logging.h"

namespace distribuild::daemon::local {

ConfigKeeper::ConfigKeeper() {
  auto channel = grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials());
  stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(stub_);

  // TODO: 启动计时器
}

std::string ConfigKeeper::GetServingDaemonToken() const {
  std::scoped_lock lock(token_mutex_);
  return serving_daemon_token_;
}

void ConfigKeeper::OnTimerFetchConfig() {
  grpc::ClientContext context;
  scheduler::GetConfigRequest  req;
  scheduler::GetConfigResponse res;

  req.set_token("123456");
  auto status = stub_->GetConfig(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("获取配置失败");
	return;
  }
  
  std::scoped_lock lock(token_mutex_);
  serving_daemon_token_ = res.daemon_token();
}

}