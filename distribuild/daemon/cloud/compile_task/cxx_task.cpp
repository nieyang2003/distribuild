#include "cxx_task.h"

#include <algorithm>

#include "distribuild/common/logging.h"
#include "distribuild/common/encode.h"
#include "distribuild/common/dir.h"
#include "distribuild/common/crypto/blake3.h"
#include "distribuild/common/crypto/zstd.h"
#include "distribuild/daemon/cloud/compilers.h"
#include "distribuild/common/string.h"

#include "file_desc.grpc.pb.h"
#include "file_desc.pb.h"

namespace distribuild::daemon::cloud {

namespace {

std::string MakeRelativeDir(const std::string& base_path, const std::string& sub_path) {
  auto result = fmt::format("{}/{}", base_path, sub_path);
  MkDir(result);
  return result.substr(base_path.length() + 1);
}

Locations FindPathLocations(const std::string& filename, const std::string& prefix) {
  Locations locations;
  auto start = filename.begin();
  while (true) {
    auto pos = std::search(start, filename.end(), prefix.begin(), prefix.end());
	if (pos == filename.end()) {
	  break;
	}
	auto end = std::search(pos, filename.end(), "\0");
    if (end == filename.end()) {
	  start = end;
	  continue;
	}

	auto added = locations.add_locations();
	added->set_position(pos - filename.begin());
	added->set_length(end - pos);
	added->set_suffix(end-pos-prefix.size());
	start = end;
  }
  return locations;
}

std::string PackOutputFiles(std::vector<std::pair<std::string, std::string>> files) {
  // TODO: 
}

}

CxxCompileTask::CxxCompileTask() : work_dir_(GetTempDir()) {}

void CxxCompileTask::OnCompleted(int exit_code, std::string std_out,
                                 std::string std_err) {
  exit_code_ = exit_code;
  stdout_ = std_out;
  stderr_ = std_err;

  // 检查结果
  auto output = GetOutput(exit_code, std_out, std_err);
  if (!output) {
	exit_code_ = -1;
	stdout_.clear();
	stderr_;
	return;
  }
  // 保存额外信息
  extra_info_ = std::move(output->extra_info);

  // 压缩文件并打包
  auto files = output->files;
  for (auto&& [filename, content] : files) {
	auto compressed_content = ZSTDCompress(content);
	content = std::move(compressed_content);
  }
  file_pack_ = PackOutputFiles(files);

  // TODO: 异步写入缓存
}

std::optional<std::string> CxxCompileTask::GetCacheKey() const {
  if (!write_cache_) {
    return std::nullopt;
  }
  return fmt::format("distribuild-cxx-cache-{}",
    EncodeHex(Blake3({env_desc_.compiler_digest(), args_, source_digest_})));
}

std::string CxxCompileTask::GetDigest() const {
  return fmt::format("distribuild-cxx-digest-{}",
    EncodeHex(Blake3({env_desc_.compiler_digest(), args_, source_digest_})));
}

std::optional<CxxCompileTask::Output> CxxCompileTask::GetOutput(int exit_code, std::string& std_out,
                                 std::string& std_err) {
  write_cache_future_.wait();
  if (exit_code != 0) {
	return std::nullopt;
  }

  CxxCompileTask::Output result;
  CxxExtraInfo extra_info;

  auto output_files = work_dir_.ReadAll();
  auto output_path_prefix = temp_sub_dir_ + "/output";

  // 取<文件名：文件>放入结果
  for (auto&& [pathname, file] : output_files) {
	DISTBU_CHECK(StartWith(pathname, output_path_prefix));
	auto filename = pathname.substr(output_path_prefix.size());

	(*extra_info.mutable_filename_infos())[filename] = FindPathLocations(filename, fmt::format("{}/{}", work_dir_.GetPath(), output_path_prefix));
	result.files.emplace_back(std::move(filename), std::move(file));
  }
  result.extra_info.PackFrom(extra_info);

  return result;
}

grpc::Status CxxCompileTask::Prepare(const QueueCxxTaskRequest& request, const std::string& attachment) {
  // 查找编译器是否存在
  auto compiler = Compilers::Instance()->TryGetPath(request.env_desc());
  if (!compiler) {
	return grpc::Status(grpc::StatusCode::NOT_FOUND, "Invalid compiler");
  }

  // 查看压缩类型
  if (request.compress_type() != CompressType::COMPRESS_TYPE_ZSTD) {
	return grpc::Status(grpc::StatusCode::NOT_FOUND, "Invalid compress type");
  }

  // 解压源码
  auto decompressed_source = ZSTDDecompress(attachment);
  if (!decompressed_source) {
	return grpc::Status(grpc::StatusCode::NOT_FOUND, "Failed to decompress source code");
  }

  // 解析请求
  source_ = std::move(*decompressed_source);
  env_desc_ = request.env_desc();
  source_path_ = request.source_path();
  args_ = request.args();
  source_digest_ = EncodeHex(Blake3({source_}));
//   write_cache_future_.;
  temp_sub_dir_ = MakeRelativeDir(work_dir_.GetPath(), source_digest_);
  cmdline_ = fmt::format("{} {} -o {}/{}/output.o", *compiler, args_, work_dir_.GetPath(), temp_sub_dir_);

  return grpc::Status::OK;
}

} // distribuild::daemon::cloud