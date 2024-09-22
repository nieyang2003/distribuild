#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>
#include <gflags/gflags.h>
#include "common/waiter.h"
#include "scheduler/scheduler_service_impl.h"
#include "scheduler/task_dispatcher.h"

DEFINE_string(service_uri, "0.0.0.0:10005", "Port the scheduler will be listening on.");

namespace distribuild::scheduler {

int StartSchduler(int argc, char** argv) {
  TaskDispatcher::Instance(); // 创建实例
  LOG_INFO("server地址：{}", FLAGS_service_uri);

  // 创建grcp server
  grpc::ServerBuilder builder;
  builder.AddListeningPort(FLAGS_service_uri, grpc::InsecureServerCredentials());
  SchedulerServiceImpl grcp_service;
  builder.RegisterService(&grcp_service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  // 等待退出
  TerminationWaiter waiter;
  waiter.run(argc, argv);

  // 等待服务器处理请求
  server->Shutdown();

  return 0;
}

} // namespace distribuild::scheduler

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  return distribuild::scheduler::StartSchduler(argc, argv);
}