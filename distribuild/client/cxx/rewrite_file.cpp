#include "rewrite_file.h"

#include "distribuild/common/logging.h"
#include "distribuild/common/string.h"

#include "distribuild/client/common/task_quota.h"
#include "distribuild/client/common/out_stream.h"
#include "distribuild/client/common/command.h"

namespace distribuild::client {

namespace {

/// @brief 返回命令行语言类型字符串
std::optional<std::string> DetermineLanguage(const CompilerArgs& args) {
  if (auto opt = args.TryGet("-x")) {
    return std::string(opt->front());
  }

  DISTBU_CHECK(args.GetFilenames().size() == 1);
  auto filename = args.GetFilenames()[0];
  if (EndWith(filename, ".cc") || EndWith(filename, ".cpp") || EndWith(filename, ".cxx")) {
    return "c++";
  } else if (EndWith(filename, ".c")) {
    return "c";
  }
  LOG_TRACE("无法从源命令中确定源代码语言: {}", args.RebuiltArg());
  return std::nullopt;
}

/// @brief 执行编译命令并返回结果
std::optional<std::tuple<std::string, CacheControl, std::string>>
TryRewriteFileWithCommandLine(const CompilerArgs& args, const RewrittenArgs& cmdline, CacheControl cache_cntl) {
  // 输出流
  std::array<OutStream*, 8> streams;
  std::size_t num_streams = 0;

  // 源码压缩输出流
  ZstdOutStream zstd_os;
  streams[num_streams++] = &zstd_os;

  // 缓存键计算流
  std::optional<Blake3OutStream> cache_os;
  if (cache_cntl != CacheControl::Disallow) {
	cache_os = Blake3OutStream();
	streams[num_streams++] = &*cache_os;
  }

  // 执行命令
  [[maybe_unused]] std::string error;
  ForwardOutStream output(std::span<OutStream*>(streams.data(), num_streams));
  int exit_code = ExecuteCommand(cmdline, {}, "", &output, &error);

  // 执行成功
  if (exit_code == 0) {
	std::string cache_key;
    if (cache_os) {
	  cache_os->Finalize();
	  cache_key = cache_os->GetResult();
	}

	return std::tuple(zstd_os.GetResult(), cache_cntl, cache_key);
  }

  // 执行失败
  return std::nullopt;
}

} // namespace

std::optional<RewriteResult> RewriteFile(const CompilerArgs& args) {
  DISTBU_CHECK(args.GetFilenames().size() == 1, "只能有一个源文件");

  auto language = DetermineLanguage(args); // 确定源代码语言
  if (!language) {
	return {};
  }
  auto cache_control = config::GetCacheControl();
  auto quota = AcquireTaskQuota(true);

  {
	RewrittenArgs cmd = args.Rewrite({"-c", "-o", "-fworking-directory"},
	                                 {},
	                                 {"-fno-working-directory", "-E", "-fdirectives-only"},
						             true);
    // 删去的选项：
	// -c: 生成目标文件（.o 或 .obj 文件）。
	// -o <file>: 指定输出文件的名称并输出到文件
	// -fworking-directory： 在调试信息中包含当前工作目录的绝对路径

    // 添加的选项：
    // -fno-working-directory: 生成调试信息时，不包含当前工作目录的绝对路径
	// -E: 只运行预处理阶段，不进行编译
	// -fdirectives-only: 删除绝对路径
	// 结果会输出到标准输出和标准错误中

	// 效果：使用相对路径，生成预处理文件，禁止输出到文件而是标准输出，再重定向到压缩流等

	auto opt = TryRewriteFileWithCommandLine(args, cmd, cache_control);
	if (opt) {
	  auto&& [rewritten, cache_cntl, digest] = *opt;
      return RewriteResult{
		.directives_only = true,
		.cache_control = cache_control,
		.language = *language,
		.source_path = args.GetFilenames()[0],
		.zstd_rewritten = std::move(rewritten),
		.source_digest = std::move(digest),
	  };
	}
	LOG_TRACE("使用编译选项\"-fdirectives-only\"编译失败，再次尝试");
  }

  // 重试，不使用"-fdirectives-only"
  {
	RewrittenArgs cmd = args.Rewrite({"-c", "-o", "-fworking-directory"},
	                                 {},
	                                 {"-fno-working-directory", "-E"},
						             true);
	auto opt = TryRewriteFileWithCommandLine(args, cmd, cache_control);
	if (opt) {
	  auto&& [rewritten, cache_cntl, digest] = *opt;
      return RewriteResult{
		.directives_only = false,
		.cache_control = cache_control,
		.language = *language,
		.source_path = args.GetFilenames()[0],
		.zstd_rewritten = std::move(rewritten),
		.source_digest = std::move(digest),
	  };
	}
  }
  LOG_TRACE("重写文件失败：{}", args.RebuiltArg());
  return std::nullopt;
}

} // namespace distribuild::client