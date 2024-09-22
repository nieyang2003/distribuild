#pragma once
#include <Poco/Timer.h>
#include "common/token_verifier.h"
#include "cache/cache_engine.h"
#include "cache/bloom_filter.h"
#include "../build/distribuild/proto/cache.grpc.pb.h"

namespace distribuild::cache {

class CacheServiceImpl : public CacheService::Service {
 public:
  CacheServiceImpl();

  // 获得缓存
  grpc::Status TryGetEntry(grpc::ServerContext* context, const TryGetEntryRequest* request,
                           grpc::ServerWriter<TryGetEntryResponseChunk>* writer) override;

  // 获得缓存
  grpc::Status PutEntry(grpc::ServerContext* context, grpc::ServerReader<PutEntryRequestChunk>* reader, 
                        PutEntryResponse* response) override;

  // 向缓存服务器请求布隆过滤器内容
  grpc::Status FetchBloomFilter(grpc::ServerContext* context, const FetchBloomFilterRequest* request,
                                FetchBloomFilterResponse* response) override;

  void Stop();
 
 private:
  std::vector<std::string> GetKeys() const;
  void OnTimerPurge(Poco::Timer& timer) { L1_cache_->Purge(); }
  void OnTimerRebuild(Poco::Timer& timer) { auto keys = GetKeys(); }

 private:
  Poco::Timer purge_timer_;
  Poco::Timer bf_rebuild_timer_;
  std::unique_ptr<TokenVerifier> user_token_verifier_;
  std::unique_ptr<TokenVerifier> servant_token_verifier_;
  std::atomic<std::uint64_t> cache_miss_{};
  std::atomic<std::uint64_t> cache_hits_{};

  std::unique_ptr<CacheEngine> L1_cache_; // L1 cache
  std::unique_ptr<CacheEngine> L2_cache_; // L2 cache
  BloomFilter bloom_filter_;
};

}