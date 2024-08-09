#include <grpcpp/create_channel.h>
#include "daemon/cloud/cache_writer.h"
#include "daemon/config.h"
#include "common/logging.h"

namespace distribuild::daemon::cloud {

CacheWriter* CacheWriter::Instance() {
  static CacheWriter instance;
  return &instance;
}

CacheWriter::CacheWriter() {
  auto channel = grpc::CreateChannel(FLAGS_cache_server_location, grpc::InsecureChannelCredentials());
  stub_ = scheduler::SchedulerService::NewStub(channel);
  DISTBU_CHECK(stub_);
}

CacheWriter::~CacheWriter() {}

std::future<bool> CacheWriter::AsyncWrite(const std::string& key, const CacheEntry& cache_entry) {
  LOG_TRACE("写入{}，当前未实现缓存", key);
  return std::future<bool>();
}

} // namespace distribuild::daemon::cloud