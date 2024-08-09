#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/impl/codegen/time.h>
#include "common/logging.h"
#include "common/tools.h"
#include "daemon/local/cache_reader.h"
#include "daemon/config.h"

using namespace std::literals;

namespace distribuild::daemon::local {

CacheReader* CacheReader::Instance() {
  static CacheReader instance;
  return &instance;
}

CacheReader::CacheReader()
  : timer_(0, 3'000) /* 3s */ {
  if (true) {
	return;
  }

  auto channel = grpc::CreateChannel(FLAGS_cache_server_location, grpc::InsecureChannelCredentials());
  stub_ = cache::CacheService::NewStub(channel);
  DISTBU_CHECK(stub_);

  // 创建成功后立即填充布隆过滤器
  OnTimerLoadBloomFilter(timer_);
  last_bf_full_update_ = last_bf_update_ = std::chrono::steady_clock::now();

  LOG_INFO("启动定时器 OnTimerLoadBloomFilter");
  timer_.start(Poco::TimerCallback<CacheReader>(*this, &CacheReader::OnTimerLoadBloomFilter));
}

CacheReader::~CacheReader() {
  timer_.stop();
}

std::optional<CacheEntry> CacheReader::TryRead(const std::string& key) {
  return std::nullopt;
  if (!stub_) {
	return std::nullopt; // 未启用缓存
  }

  {
	std::scoped_lock lock(bf_mutex_);
	if (last_bf_update_ > std::chrono::steady_clock::now() + 10min ||
	    !bloom_filter_.PossiblyContains(key)) { // 超时或不存在
	  return std::nullopt;
	}
  }
  
  //可能命中，准备获取
  grpc::ClientContext context;
  cache::TryGetEntryRequest  req;
  grpc::CompletionQueue cq;
  cache::TryGetEntryResponse resp;
  
  SetTimeout(&context, 10s);
  req.set_token(FLAGS_cache_server_token);
  req.set_key(key);
  
  auto status = stub_->AsyncTryGetEntry(&context, req, &cq);
  // TODO: 异步操作
  
  return std::nullopt;
}

void CacheReader::OnTimerLoadBloomFilter(Poco::Timer& timer) {
  auto now = std::chrono::steady_clock::now();

  grpc::ClientContext context;
  cache::FetchBloomFilterRequest  req;
  cache::FetchBloomFilterResponse resp;

  SetTimeout(&context, 10s);
  req.set_token(FLAGS_cache_server_token);
  {
	std::scoped_lock lock(bf_mutex_);
	if (last_bf_full_update_.time_since_epoch() == 0s) {
      // 初次强制更新
	  req.set_secs_last_fetch(0x7fff'ffff);
	  req.set_secs_last_full_fetch(0x7fff'ffff);
	} else {
	  req.set_secs_last_fetch((now - last_bf_update_) / 1s);
	  req.set_secs_last_full_fetch((now - last_bf_full_update_) / 1s);
	}
  }
  
  auto status = stub_->FetchBloomFilter(&context, req, &resp);
  if (!status.ok()) {
	LOG_WARN("获取布隆过滤器失败：{}", status.error_details());
	return;
  }
  last_bf_update_ = now;

  if (resp.incremental()) {
	// 增量更新
	for (auto&& e : resp.newly_populated_keys()) {
	  std::scoped_lock lock(bf_mutex_);
	  bloom_filter_.Add(e);
	}
	LOG_INFO("更新了{}个新的键", resp.newly_populated_keys_size());
  } else {
    // 全量更新
	last_bf_full_update_ = now;
	// TODO: bytes解析
	// TODO: 流式传输或元数据
  }
}

} // namespace distribuild::daemon::local