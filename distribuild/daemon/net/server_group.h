#pragma once

#include <vector>
#include <memory>

namespace grpc {

class Server;

} // namespace grpc

namespace distribuild::daemon {

using Server = grpc::Server;
using ServerPtr = std::unique_ptr<Server>;

class ServerGroup {
 public:
  ServerGroup();
  ~ServerGroup();

  Server* AddServer();
  void AddServer(ServerPtr);
  void Start();
  void Stop();
  void Join();

 private:
  std::vector<ServerPtr> servers_;
};

}