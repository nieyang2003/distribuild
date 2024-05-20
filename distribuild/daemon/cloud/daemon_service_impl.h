#pragma once

#include "daemon.grpc.pb.h"
#include "daemon.pb.h"

namespace distribuild::daemon::cloud {

class DaemonServiceImpl : public DaemonService::Service {
 public:
  explicit DaemonServiceImpl(std::string address);
  void Join();
  void Stop();

 public:
  grpc::Status QueueCxxTask(grpc::ServerContext* context,
      const QueueCxxTaskRequest* request, QueueCxxTaskResponse* response) override;

  grpc::Status AddTaskRef(grpc::ServerContext* context,
      const AddTaskRefRequest* request, AddTaskRefResponse* response) override;
  grpc::Status FreeTask(grpc::ServerContext* context,
      const FreeTaskRequest* request, FreeTaskResponse* response) override;

  grpc::Status WaitForTask(grpc::ServerContext* context,
      const WaitForTaskRequest* request, WaitForTaskResponse* response) override;

 private:
  
 private:
  std::string address_;
  // TODO: TOKEN
};

} // namespace distribuild::daemon::cloud