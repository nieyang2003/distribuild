#include <grpcpp/create_channel.h>
#include <Poco/ThreadPool.h>
#include <Poco/Task.h>
#include "daemon/cloud/cache_writer.h"
#include "daemon/config.h"
#include "common/spdlogging.h"
#include "common/tools.h"

namespace distribuild::daemon::cloud {

namespace {

class WriteCacheDataPocoTask : public Poco::Task {
  cache::CacheService::Stub* stub_;
  std::string key_;
  std::string data_;
  std::promise<bool> promise_;
 public:
  WriteCacheDataPocoTask(std::future<bool>* future, cache::CacheService::Stub* stub, std::string key, std::string&& data)
    : Poco::Task("WriteCacheDataPocoTask")
    , stub_(stub)
    , key_(key)
    , data_(std::move(data)) {
    *future = promise_.get_future();
  }

  virtual void runTask() override {
    LOG_DEBUG("开始写入缓存");
    grpc::ClientContext context;
	auto* req = new cache::PutEntryRequest;
    cache::PutEntryResponse resp;

	req->set_key(key_);
	req->set_token(FLAGS_cache_server_token);
	SetTimeout(&context, 5s);

	cache::PutEntryRequestChunk chunk;
	chunk.set_allocated_request(req);

    auto writer = stub_->PutEntry(&context, &resp);
	if (!writer) {
	  LOG_ERROR("失败");
      promise_.set_value(false);
	}
	if (!writer->Write(chunk)) {
      LOG_ERROR("PutEntry写入第一个块失败");
      promise_.set_value(false);
    }
    for (std::size_t i = 0; i < data_.size(); i += FLAGS_chunk_size) {
  	  chunk.clear_request();
  	  size_t remaining_size = data_.size() - i;
  	  chunk.set_file_chunk(data_.data() + i, std::min(FLAGS_chunk_size, remaining_size));
      if (!writer->Write(chunk)) {
  	    LOG_ERROR("PutEntry写入文件失败");
        promise_.set_value(false);
  	  }
    }
	writer->WritesDone();
    grpc::Status status = writer->Finish();
    if (!status.ok()) {
      LOG_WARN("RCP调用`PutEntry`失败：{}", status.error_message());
      promise_.set_value(false);
    }
	promise_.set_value(true);
  }
};

} // namespace

CacheWriter* CacheWriter::Instance() {
  static CacheWriter instance;
  return &instance;
}

CacheWriter::CacheWriter()
  : task_manager_(Poco::ThreadPool::defaultPool()) {
  if (FLAGS_cache_server_location.empty()) {
	return;
  }
  auto channel = grpc::CreateChannel(FLAGS_cache_server_location, grpc::InsecureChannelCredentials());
  stub_ = cache::CacheService::NewStub(channel);
  DISTBU_CHECK(stub_);
}

CacheWriter::~CacheWriter() {}

std::optional<std::future<bool>> CacheWriter::AsyncWrite(const std::string& key, CacheEntry&& cache_entry) {
  if (!stub_) {
	LOG_DEBUG("缓存未启用");
  }
  if (cache_entry.exit_code != 0) {
	return std::nullopt;
  }

  std::future<bool> result;
  auto data = TryMakeCacheData(std::move(cache_entry));
  if (!data) {
	return std::nullopt;
  }
  task_manager_.start(new WriteCacheDataPocoTask(&result, stub_.get(), key, std::move(*data)));
  return result;
}

} // namespace distribuild::daemon::cloud