#include "cache_reader.h"

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel.h>

#include "distribuild/common/logging.h"

using namespace std::literals;

namespace distribuild::daemon::local {

CacheReader* CacheReader::Instance() {
  static CacheReader instance;
  return &instance;
}

CacheReader::CacheReader() {
  if (true) {
	return;
  }

  auto channel = grpc::CreateChannel("127.0.0.1:10000", grpc::InsecureChannelCredentials());
  stub_ = cache::CacheService::NewStub(channel);
  DISTBU_CHECK(stub_);

  // 创建成功后立即填充布隆过滤器
  OnTimerLoadBloomFilter();
  last_bf_full_update_ = last_bf_update_ = std::chrono::steady_clock::now();

  // TODO: 启动定时器，OnTimerLoadBloomFilter
}

CacheReader::~CacheReader() {
  // TODO: 注销定时器
}

std::optional<CacheEntry> CacheReader::TryRead(const std::string& key) {
  if (!stub_) {
	return std::nullopt;
  }

  {
	std::scoped_lock lock(bf_mutex_);
	if (last_bf_update_ > std::chrono::steady_clock::now() + 10min ||
	    !bloom_filter_.PossiblyContains(key)) { // 超时或不存在
	  return std::nullopt;
	}
  }

  grpc::ClientContext context;
  cache::TryGetEntryRequest  req;
  grpc::CompletionQueue cq;
  cache::TryGetEntryResponse res;
  
  context.set_deadline(std::chrono::steady_clock::now() + 10s);
  req.set_token("123456");
  req.set_key(key);
  
  auto status = stub_->AsyncTryGetEntry(&context, req, &cq);
  // TODO: 异步操作
  
}

void CacheReader::OnTimerLoadBloomFilter() {
  auto now = std::chrono::steady_clock::now();
  grpc::ClientContext context;
  cache::FetchBloomFilterRequest  req;
  cache::FetchBloomFilterResponse res;

  context.set_deadline(now + 10s);
  req.set_token("123456");
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
  
  auto status = stub_->FetchBloomFilter(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("获取布隆过滤器失败：{}", status.error_details());
	return;
  }
  last_bf_update_ = now;

  if (res.incremental()) {
	// 增量更新
	for (auto&& e : res.newly_populated_keys()) {
	  std::scoped_lock lock(bf_mutex_);
	  bloom_filter_.Add(e);
	}
	LOG_INFO("更新了{}个新的键", res.newly_populated_keys_size());
  } else {
    // 全量更新
	last_bf_full_update_ = now;
	// TODO: bytes解析
	// TODO: 流式传输或元数据
  }
}

} // namespace distribuild::daemon::local