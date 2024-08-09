#include "daemon/local/dist_task/cxx_task.h"
#include "common/logging.h"
#include "common/encode.h"
#include "common/crypto/blake3.h"
#include "daemon/local/file_cache.h"
#include "common/tools.h"
#include "../build/distribuild/proto/daemon.grpc.pb.h"
#include "../build/distribuild/proto/daemon.pb.h"
#include "daemon/config.h"

using namespace std::literals;

namespace distribuild::daemon::local {

std::string CxxDistTask::CacheKey() const {
  return fmt::format("distribuild-cxx-cache-{}",
    EncodeHex(Blake3({env_desc_.compiler_digest(), args_, source_digest_})));
}

std::string CxxDistTask::GetDigest() const {
  return fmt::format("distribuild-cxx-digest-{}",
    EncodeHex(Blake3({env_desc_.compiler_digest(), args_, source_digest_})));
}

std::optional<std::uint64_t> CxxDistTask::StartTask(cloud::DaemonService::Stub* stub, const std::string& token, std::uint64_t grant_id) {
  grpc::ClientContext context;
  SetTimeout(&context, 30s);
  cloud::QueueCxxTaskRequestChunk chunk;
  cloud::QueueCxxTaskResponse resp;
  auto writer = stub->QueueCxxTask(&context, &resp);
  // 设置第一个请求
  cloud::QueueCxxTaskRequest* req = new cloud::QueueCxxTaskRequest;
  req->set_token(token);
  req->set_task_grant_id(grant_id);
  req->set_source_path(source_digest_);
  req->set_args(args_);
  req->set_compress_type(cloud::CompressType::COMPRESS_TYPE_ZSTD);
  req->set_fill_cache(cache_control_);
  *req->mutable_env_desc() = env_desc_;

  chunk.set_allocated_request(req);
  if (!writer->Write(chunk)) {
	LOG_ERROR("QueueCxxTask写入第一个块失败");
    return false;
  }

  // 写入文件
  auto&& file = source_;
  for (std::size_t i = 0; i < file.size(); i += FLAGS_chunk_size) {
	chunk.clear_request();
	size_t remaining_size = file.size() - i;
	chunk.set_file_chunk(file.data() + i, std::min(FLAGS_chunk_size, remaining_size));
    if (!writer->Write(chunk)) {
	  LOG_ERROR("QueueCxxTask 写入文件失败");
      return false;
	}
  }
  source_.clear();

  // 获取返回结果
  writer->WritesDone();
  grpc::Status status = writer->Finish();
  if (!status.ok()) {
	LOG_WARN("RCP调用`QueueCxxTask`失败");
	return std::nullopt;
  }

  return resp.task_id();
}

grpc::Status CxxDistTask::Prepare(const http_service::SubmitCxxTaskRequest& req,
                                  const std::vector<std::string_view>& bytes) {
  // 检查参数是否合法
  if (req.requestor_pid() <= 1 || req.source_path().empty() ||
      req.compiler_args().empty() || req.source_digest().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "参数错误");
  }

  // 查询编译器是否存在
  auto compiler = FileCache::Instance()->TryGet(req.compiler().path(), req.compiler().size(), req.compiler().mtime());
  if (!compiler) {
	return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "编译器不存在");
  }

  // update
  cache_control_ = !!req.cache_control();
  requester_pid_ = req.requestor_pid();
  env_desc_.set_compiler_digest(*compiler);
  source_path_ = req.source_digest();
  source_digest_ = req.source_digest();
  args_ = req.compiler_args();
  source_ = std::move(bytes[0]); // ! 多个文件

  return grpc::Status::OK;
}

std::optional<CxxDistTask::Output> CxxDistTask::RebuildOutput(DistOutput&& output) {
  LOG_DEBUG("RebuildOutput：exit_code = {}, out = {}, err = {}", output.exit_code, output.std_out, output.std_err);
  http_service::WaitForCXXTaskResponse res;

  res.set_exit_code(output.exit_code);
  res.set_std_out(std::move(output.std_out));
  res.set_std_err(std::move(output.std_err));

  std::vector<std::string> buffers;
  if (res.exit_code() < 0) {
	return std::make_pair(std::move(res), std::move(buffers));
  }

  for (auto&& [suffix, content] : output.output_files) {
	res.add_file_extensions(std::move(suffix));
	buffers.push_back(std::move(content));
  }

  return std::make_pair(std::move(res), std::move(buffers));
}

} // namespace distribuild::daemon::local
