#include "cxx_task.h"

#include "distribuild/common/logging.h"
#include "distribuild/common/encode.h"
#include "distribuild/common/crypto/blake3.h"

#include "distribuild/daemon/local/file_cache.h"

#include "daemon.grpc.pb.h"
#include "daemon.pb.h"

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

std::optional<std::uint64_t> CxxDistTask::StartTask(
    cloud::DaemonService::Stub* stub,
	const std::string& token,
    std::uint64_t grant_id) {
  grpc::ClientContext context;
  cloud::QueueCxxTaskRequest  req;
  cloud::QueueCxxTaskResponse res;

  req.set_token(token);
  req.set_task_grant_id(grant_id);
  *req.mutable_env_desc() = env_desc_;
  req.set_source_path(source_digest_);
  req.set_args(args_);
  req.set_compress_type(cloud::CompressType::COMPRESS_TYPE_ZSTD);
  req.set_disfill_cache(!cache_control_);

  context.set_deadline(std::chrono::steady_clock::now() + 30s);
  context.AddMetadata("attachment", source_);
  source_.shrink_to_fit();
  
  // 放入任务
  auto status = stub->QueueCxxTask(&context, req, &res);
  if (!status.ok()) {
	LOG_WARN("RCP调用`QueueCxxTask`失败");
	return std::nullopt;
  }

  return res.task_id();
}

grpc::Status CxxDistTask::Prepare(const http_service::SubmitCxxTaskRequest& req,
                                  const std::vector<std::string_view>& bytes) {
  // 检查参数是否合法
  if (req.requestor_pid() <= 1 || req.source_path().empty() ||
      req.compiler_args().empty() || req.source_digest().empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid arguments.");
  }

  // 查询编译器是否存在
  auto compiler = FileCache::Instance()->TryGet(req.compiler().path(),
                                                req.compiler().size(),
												req.compiler().timestamp());
  if (!compiler) {
	return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid compiler.");
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

std::optional<CxxDistTask::Output> CxxDistTask::RebuildOutput(const DistOutput& output) {
  http_service::WaitForCXXTaskResponse res;

  res.set_exit_code(output.exit_code);
  res.set_std_out(output.std_out);
  res.set_std_err(output.std_err);

  std::vector<std::string> buffers;
  if (res.exit_code() < 0) {
	return std::pair(res, buffers);
  }

  for (auto&& e : output.output_files) {
	res.add_file_extensions(e.first);
	buffers.push_back(e.second);
  }

  return std::pair(res, buffers);
}

} // namespace distribuild::daemon::local
