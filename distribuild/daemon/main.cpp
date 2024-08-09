#include <grpcpp/server_builder.h>
#include <Poco/ThreadPool.h>
#include <Poco/Net/HTTPServer.h>
#include "common/waiter.h"
#include "daemon/privilege.h"
#include "daemon/config.h"
#include "daemon/sysinfo.h"
#include "daemon/cloud/temp_dir.h"
#include "daemon/cloud/cache_writer.h"
#include "daemon/cloud/compilers.h"
#include "daemon/cloud/executor.h"
#include "daemon/cloud/daemon_service_impl.h"
#include "daemon/local/cache_reader.h"
#include "daemon/local/file_cache.h"
#include "daemon/local/task_monitor.h"
#include "daemon/local/task_dispatcher.h"
#include "daemon/local/http_service_impl.h"

namespace distribuild::daemon {

int StartServer(int argc, char** argv) {
  // 解析命令行
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // 设置环境
  setenv("LC_ALL", "en_US.utf8", true);
  unsetenv("GCC_COMPARE_DEBUG");
  unsetenv("SOURCE_DATE_EPOCH");

  // 退出root权限
  DropPrivilege();

  // 禁用core dump
  if (!FLAGS_allow_core_dump) {
	DisableCoreDump();
  }

  // 选用工作目录
  LOG_INFO("工作目录：'{}'", cloud::GetTempDir());

  // 清除非正常退出内容

  // 设置线程池线程数量
  Poco::ThreadPool::defaultPool().addCapacity(GetNumCPUCores());

  // 初始化单例
  (void)cloud::CacheWriter::Instance();
  (void)cloud::Compilers::Instance();
  (void)cloud::Executor::Instance();
  (void)local::TaskDispatcher::Instance();
  (void)local::CacheReader::Instance();
  (void)local::CacheReader::Instance();
  (void)local::FileCache::Instance();
  (void)local::TaskMonitor::Instance();

  LOG_INFO("缓存服务器地址: {}", FLAGS_cache_server_location);

  // 启动grpc进程
  grpc::ServerBuilder builder;
  builder.AddListeningPort(FLAGS_servant_location, grpc::InsecureServerCredentials());
  cloud::DaemonServiceImpl daemon_service(FLAGS_servant_location);
  builder.RegisterService(&daemon_service);
  std::unique_ptr<grpc::Server> rpc_server(builder.BuildAndStart());
  LOG_INFO("cloud grpc服务启动");

  // http进程
  Poco::Net::HTTPServerParams * params = new Poco::Net::HTTPServerParams;
  params->setMaxThreads(Poco::ThreadPool::defaultPool().capacity());
  params->setMaxQueued(100);
  Poco::Net::HTTPServer http_server(new local::HttpFactory(), Poco::ThreadPool::defaultPool(), Poco::Net::ServerSocket(8080), params);
  http_server.start();
  LOG_INFO("local http服务启动");

  // 等待与关闭
  TerminationWaiter waiter;
  waiter.run(argc, argv);

  LOG_INFO("退出...");
  daemon_service.Stop();
  cloud::Compilers::Instance()->Stop();
  cloud::Executor::Instance()->Stop();
  local::TaskDispatcher::Instance()->Stop();

  http_server.stop();
  rpc_server->Shutdown();

  return 0;
}

}

int main(int argc, char** argv) {
  distribuild::daemon::StartServer(argc, argv);
}