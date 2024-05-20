#include "server_group.h"

#include <grpcpp/server.h>

namespace distribuild::daemon {

ServerGroup::ServerGroup() = default;

ServerGroup::~ServerGroup() = default;

Server* ServerGroup::AddServer() {
  servers_.push_back(std::make_unique<Server>());
  return servers_.back().get();
}

void ServerGroup::AddServer(ServerPtr server) {
  servers_.push_back(std::move(server));
}

void ServerGroup::Start() {
  for (auto&& server : servers_) {
	server->Start();
  }
}

void ServerGroup::Stop() {
  for (auto&& server : servers_) {
	server->Shutdown();
  }
}

void ServerGroup::Join() {
  for (auto&& server : servers_) {
	server->Wait();
  }
}

} // namespace distribuild::daemon