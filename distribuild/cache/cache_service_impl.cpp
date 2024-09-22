#include <gflags/gflags.h>
#include "cache/cache_service_impl.h"
#include "common/spdlogging.h"
#include "cache_service_impl.h"

DEFINE_uint64(chunk_size, 1 * 1024 * 1024, "发送文件的分块大小，默认1M");

DEFINE_string(user_token, "nieyang", "local使用的token");
DEFINE_string(servant_token, "nieyang", "daemon cloud使用的token");

namespace distribuild::cache {

CacheServiceImpl::CacheServiceImpl()
  : purge_timer_(0, 1'000)
  , bf_rebuild_timer_(0, 60'000) {
  // TODO: 创建cache
  user_token_verifier_ = MakeTokenVerifier(FLAGS_user_token);
  servant_token_verifier_ = MakeTokenVerifier(FLAGS_servant_token);
  purge_timer_.start(Poco::TimerCallback<CacheServiceImpl>(*this, &CacheServiceImpl::OnTimerPurge));
  bf_rebuild_timer_.start(Poco::TimerCallback<CacheServiceImpl>(*this, &CacheServiceImpl::OnTimerRebuild));
}

grpc::Status CacheServiceImpl::TryGetEntry(grpc::ServerContext *context, 
  const TryGetEntryRequest *request, grpc::ServerWriter<TryGetEntryResponseChunk> *writer) {
  LOG_DEBUG("调用者：`{}`", context->peer());

  if (!user_token_verifier_->Verify(request->token())) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
  }

  auto bytes = L1_cache_->TryGet(request->key());
  if (!bytes) {
	bytes = L2_cache_->TryGet(request->key());
  }

  if (!bytes) {
	cache_miss_.fetch_add(1, std::memory_order_relaxed);
	return grpc::Status(grpc::StatusCode::NOT_FOUND, "Cache miss");
  }

  cache_hits_.fetch_add(1, std::memory_order_relaxed);
  L2_cache_->Put(request->key(), *bytes);

  for (std::size_t i = 0; i < bytes->size(); i += FLAGS_chunk_size) {
	TryGetEntryResponseChunk chunk;
	size_t remaining_size = bytes->size() - i;
	chunk.set_file_chunk(bytes->data() + i, std::min(FLAGS_chunk_size, remaining_size));
    writer->Write(chunk);
  }

  return grpc::Status::OK;
}

grpc::Status CacheServiceImpl::PutEntry(grpc::ServerContext *context,
  grpc::ServerReader<PutEntryRequestChunk> *reader, PutEntryResponse *response) {
  LOG_DEBUG("调用者：`{}`", context->peer());

  PutEntryRequestChunk chunk;
  std::unique_ptr<PutEntryRequest> request;
  std::string file;
  bool is_first_chunk = true;

  while (reader->Read(&chunk)) {
    if (is_first_chunk) [[unlikely]] {
	  // 处理第一个块，请求
	  if (!chunk.has_request() || chunk.file_chunk().size() != 0) [[unlikely]] {
		return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "rpc格式错误");
	  }

	  request.reset(chunk.release_request());
	  if (!servant_token_verifier_->Verify(request->token())) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Token验证失败");
      }

	  is_first_chunk = false;
	} else [[likely]] {
	  // 处理后续块，文件
      if (chunk.has_request()) [[unlikely]] {
		return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "rpc格式错误");
	  }

	  file.append(chunk.file_chunk().data(), chunk.file_chunk().size());
	}
  }

  LOG_INFO("写入缓存: {}；大小：{}", request->key(), file.size());
  L1_cache_->Put(request->key(), file);
  L2_cache_->Put(request->key(), file);
  bloom_filter_.Add(request->key());

  return grpc::Status::OK;
}

grpc::Status CacheServiceImpl::FetchBloomFilter(grpc::ServerContext *context,
  const FetchBloomFilterRequest *request, FetchBloomFilterResponse *response) {
  // TODO:
  return grpc::Status();
}

void CacheServiceImpl::Stop() {
  purge_timer_.stop();
  bf_rebuild_timer_.stop();
}

std::vector<std::string> CacheServiceImpl::GetKeys() const {
  auto result = L1_cache_->GetKeys();
  auto L2_keys = L2_cache_->GetKeys();
  result.insert(result.end(), L2_keys.begin(), L2_keys.end());
  return result;
}

}