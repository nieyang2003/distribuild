#include <algorithm>
#include <Poco/ThreadPool.h>
#include <Poco/Task.h>
#include "common/spdlogging.h"
#include "common/encode.h"
#include "common/dir.h"
#include "common/crypto/blake3.h"
#include "common/crypto/zstd.h"
#include "common/tools.h"
#include "daemon/cloud/compile_task/cxx_task.h"
#include "daemon/cloud/compilers.h"
#include "daemon/cloud/cache_writer.h"
#include "../build/distribuild/proto/file_desc.grpc.pb.h"
#include "../build/distribuild/proto/file_desc.pb.h"
#include "cxx_task.h"

namespace distribuild::daemon::cloud {

namespace {

static constexpr std::string_view kRejectMacros[] = {"__TIME__", "__DATE__", "__TIMESTAMP__"};

/// @brief 搜索源码中是否有时间相关宏，有则不能缓存
class PrepareCachePocoTask : public Poco::Task {
  std::promise<bool> promise_;
  const std::string& source_;

 public:
   PrepareCachePocoTask(std::optional<std::future<bool>>& future, const std::string& source)
     : Poco::Task("PrepareCachePocoTask")
	 , source_(source) {
     future = promise_.get_future();
   }
   virtual void runTask() override {
     for (auto&& macro : kRejectMacros) {
       if (std::search(source_.begin(), source_.end(), macro.begin(), macro.end()) != source_.end()) {
         promise_.set_value(false);
       }
     }
     promise_.set_value(true);
   }
};

/// @brief 创建临时目录
std::string MakeRelativeDir(const std::string& base_path, const std::string& dir_name) {
  auto result = fmt::format("{}/{}", base_path, dir_name);
  Mkdirs(result);
  return result.substr(base_path.length() + 1);
}

Locations FindPathLocations(const std::string& file, const std::string& prefix) {
  static constexpr auto kTerminateNull = "\0"sv;

  Locations locations;
  auto start = file.begin();
  while (true) {
	// 找到文件前缀开始结束位置
    auto pos = std::search(start, file.end(), prefix.begin(), prefix.end());
	if (pos == file.end()) {
	  break;
	}

    // 找到空字节位置
	auto end = std::search(pos, file.end(), kTerminateNull.begin(), kTerminateNull.end());
    if (end == file.end() || end - pos > PATH_MAX) {
	  LOG_WARN("查找location错误");
	  start = end;
	  continue;
	}

	auto added = locations.add_locations();
	added->set_position(pos - file.begin());
	added->set_length(end - pos);
	added->set_suffix(end - pos - prefix.size());
	start = end;
  }
  return locations;
}

} // namespace

CxxCompileTask::CxxCompileTask()
  : task_manager_(Poco::ThreadPool::defaultPool())
  , work_dir_(GetTempDir()) {
  LOG_DEBUG("工作目录：{}", work_dir_.GetPath());
}

void CxxCompileTask::OnCompleted(int exit_code, std::string&& std_out, std::string&& std_err) {
  exit_code_ = exit_code;
  stdout_ = std::move(std_out);
  stderr_ = std::move(std_err);

  // 检查结果
  auto output = GetOutput(exit_code, std_out, std_err);
  if (!output) {
	exit_code_ = -1;
	stdout_.clear();
	stderr_.clear();
	return;
  }
  // 保存额外信息
  extra_info_ = std::move(output->extra_info);

  // 压缩文件并打包
  auto files = output->files;
  for (auto&& [filename, content] : files) {
	LOG_DEBUG("filename = {}, content size = {}", filename, content.size());
	auto compressed_content = ZSTDCompress(content);
	DISTBU_CHECK(compressed_content);
	content = std::move(*compressed_content);
  }
  file_pack_ = PackFiles(files);
  LOG_DEBUG("打包后，file_pack_ size = {}, files size = {}", file_pack_.size(), files.size());

  if (auto key = GetCacheKey(); key && exit_code == 0) {
	LOG_DEBUG("写入缓存");
	// 复制，不用担心写入析构，不需要string_view
	// ! 复制太多
	CacheEntry entry = {.exit_code  = exit_code_,
	                    .std_out    = stdout_,
						.std_err    = stderr_,
						.extra_info = extra_info_,
						.packed     = file_pack_ };
	CacheWriter::Instance()->AsyncWrite(*key, std::move(entry));
  }
}

std::optional<std::string> CxxCompileTask::GetCacheKey() const {
  if (!write_cache_future_ || write_cache_future_->get()) {
    return std::nullopt;
  }
  return fmt::format("distribuild-cxx-cache-{}",
    EncodeHex(Blake3({env_desc_.compiler_digest(), args_, source_digest_})));
}

std::string CxxCompileTask::GetDigest() const {
  return fmt::format("distribuild-cxx-digest-{}",
    EncodeHex(Blake3({env_desc_.compiler_digest(), args_, source_digest_})));
}

std::optional<CxxCompileTask::Output> CxxCompileTask::GetOutput(int exit_code, std::string& std_out, std::string& std_err) {
  if (write_cache_future_) {
	write_cache_future_->wait();
  }

  CxxCompileTask::Output result;

  // 编译失败
  if (exit_code != 0) {
	return result;
  }

  CxxExtraInfo extra_info;

  // 读取生成的所有文件
  auto output_files = work_dir_.ReadAll();
  LOG_DEBUG("读取了{}个文件", output_files.size());

  // 目标文件前缀
  auto output_path_prefix = temp_sub_dir_ + "/output";

  // 取<文件名：文件>放入结果
  for (auto&& [filename, file] : output_files) {
	LOG_DEBUG("读取文件 `{}`", filename);
	// 检查生成的文件绝对正确
	DISTBU_CHECK(StartWith(filename, output_path_prefix));
	// 取文件后缀名
	auto suffix = filename.substr(output_path_prefix.size());
    // 设置
	(*extra_info.mutable_filename_infos())[suffix] = FindPathLocations(file, fmt::format("{}/{}", work_dir_.GetPath(), output_path_prefix));
    // 放入[后缀名-文件]
	result.files.emplace_back(std::move(suffix), std::move(file));
  }
  result.extra_info.PackFrom(extra_info);

  return result;
}

grpc::Status CxxCompileTask::Prepare(const QueueCxxTaskRequest& request, const std::string& file) {
  // 查找编译器是否存在
  auto compiler = Compilers::Instance()->TryGetPath(request.env_desc());
  if (!compiler) {
	return grpc::Status(grpc::StatusCode::NOT_FOUND, "编译器不存在");
  }

  // 查看压缩类型
  if (request.compress_type() != CompressType::COMPRESS_TYPE_ZSTD) {
	return grpc::Status(grpc::StatusCode::NOT_FOUND, "不支持的压缩类型");
  }

  // 解压源码
  auto decompressed_source = ZSTDDecompress(file);
  if (!decompressed_source) {
	return grpc::Status(grpc::StatusCode::NOT_FOUND, "解压缩源码失败");
  }
  LOG_DEBUG("解压缩后大小：{}", decompressed_source->size());

  // 解析请求
  source_ = std::move(*decompressed_source);
  env_desc_ = request.env_desc();
  source_path_ = request.source_path();
  args_ = request.args();
  source_digest_ = EncodeHex(Blake3({source_}));
  if (request.fill_cache()) {
	PrepareCache();
  }
  temp_sub_dir_ = MakeRelativeDir(work_dir_.GetPath(), source_digest_);
  cmdline_ = fmt::format("{} {} -o {}/{}/output.o", *compiler, args_, work_dir_.GetPath(), temp_sub_dir_);

  return grpc::Status::OK;
}

void CxxCompileTask::PrepareCache() {
  if (std::all_of(std::begin(kRejectMacros), std::end(kRejectMacros), [&](auto&& macro) {
    return args_.find(fmt::format("-D{}=", macro)) != std::string_view::npos;
  })) {
    return;
  }
  task_manager_.start(new PrepareCachePocoTask(write_cache_future_, source_));
}

} // distribuild::daemon::cloud