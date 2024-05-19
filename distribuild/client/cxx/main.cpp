#include "distribuild/common/logging.h"
#include "distribuild/common/string.h"

#include "distribuild/client/common/utility.h"
#include "distribuild/client/common/command.h"
#include "distribuild/client/common/task_quota.h"
#include "distribuild/client/common/io.h"
#include "distribuild/client/cxx/compilition.h"

#include "compiler_args.h"
#include "rewrite_file.h"

namespace distribuild::client {

using namespace std::literals;

/// @brief 根据参数判断是否可以在远程执行
bool IsDistributable(const CompilerArgs& args) {
  if (!args.TryGet("-c")) {
	LOG_TRACE("不可用选项 '-c'，退出");
	return false;
  }
  // TODO
  return true;
}

/// @brief 根据参数判断是否为轻量级任务
bool IsLightweight(const CompilerArgs& args) {
  static constexpr std::string_view kLightweightArgs[] = {"-dumpversion", "-dumpmachine", "-E"};

  for (auto&& e : kLightweightArgs) {
	if (args.TryGet(e)) {
	  return true;
	}
  }
  return false;
}

/// @brief 将编译结果写入文件
void WriteCompileResult(const std::string& destination, const std::vector<std::pair<std::string, std::string>>& output_files) {
  // 获得文件名 xxx.o
  std::string prefix = destination;
  if (prefix.size() >= 2 && prefix.substr(prefix.size() - 2) == ".o") {
	prefix = prefix.substr(prefix.size() - 2);
  }
  // 写入
  for (auto&& [suffix, file] : output_files) {
	LOG_TRACE("文件扩展名：{}，{}字节", suffix, file.size());
    if (suffix == ".o") {
      WriteAll(destination, file);
	} else { // 不为.o扩展名的输出文件
	  WriteAll(prefix + suffix, file);
	}
  }
}

/// @brief 编译程序主流程
int Compile(int argc, const char* argv[]) {
  // distribuild g++ ...
  bool use_this = EndWith(argv[0], "distribuild");
  int skip = use_this ? 2 : 1;

  CompilerArgs args(argc - skip, argv + skip);

  // 找到实际编译器绝对路径
  if (use_this && argv[1][0] == '/') {
	// distribuild /usr/bin/.../g++ 
    args.SetCompiler(argv[1]); // 使用了绝对路径
  } else {
    args.SetCompiler(FindExecutableInPath(GetBaseName(argv[skip - 1]), [](auto&& path) {
		return !EndWith(path, "ccache");
	}));
  }
  LOG_TRACE("使用编译器: {}", args.GetCompiler());

  // 编译仿函数
  auto compile_on_native = [&](auto&& quota) -> int {
    return CompileOnNativeUsingQuota(args.GetCompiler(), argv + skip, quota);
  };
  auto compile_on_native_using_quota = [&] -> int {
    return compile_on_native(AcquireTaskQuota(IsLightweight(args)));
  };

  // 是否可在云端执行
  if (!IsDistributable(args)) {
	LOG_TRACE("无法远程执行，开始本地执行");
    return compile_on_native_using_quota();
  }

  // 处理源文件
  auto rewritten = RewriteFile(args);
  if (!rewritten) { // 处理错误
	LOG_INFO("参数错误，无法重写文件，开始本地执行: {}", args.RebuiltArg());
    return compile_on_native_using_quota();
  }

  // 编译缓存
  if (rewritten->cache_control == CacheControl::Disallow
      && GetCacheControlEnv() != CacheControl::Disallow) {  // 环境允许但重写结果不允许
	LOG_WARN("存在不可缓存的文件。参数: {}", args.RebuiltArg());
  }

  // 小文件
  if (rewritten->zstd_rewritten.size() < 8192) {
	LOG_TRACE("预处理文件太小，本地编译速度可能更快");
	return compile_on_native_using_quota();
  }

  // 编译
  int retries = 5;
  while (true) {
    CompileResult compile_result = CompileOnCloud(args, std::move(*rewritten));
	if (compile_result.exit_code < 0 || compile_result.exit_code == 127) { // 失败或编译器错误
	  if (auto quota = TryAcquireTaskQuota(false, 10s)) {
		LOG_INFO("云端编译失败，尝试本地编译。exit_code = {}", compile_result.exit_code);
		return compile_on_native(quota);
	  }

	  // 尝试重新提交
	  if (retries--) {
		LOG_TRACE("在云端编译失败, exit_code = {}, 重试", compile_result.exit_code);

		if (rewritten = RewriteFile(args)) { // 被move所以重新生成
		  continue;
		}
	  }
	}
	if (compile_result.exit_code != 0) {
	  LOG_DEBUG("在云端编译失败");
	  if (compile_result.exit_code == 1) {
		LOG_TRACE("云端编译失败, exit_code = {}, 尝试本地编译: {}", compile_result.exit_code, args.RebuiltArg());
	  } else {
		LOG_WARN("未知错误码, exit_code = {}, 尝试本地编译", compile_result.exit_code);
	  }
	  return compile_on_native_using_quota();
	}

	LOG_TRACE("云端编译成功");
	fprintf(stdout, "%s", compile_result.out_put.c_str());
	fprintf(stderr, "%s", compile_result.error.c_str());
	WriteCompileResult(args.GetOutputFile(), compile_result.output_files);
	break;
  }

  return 0;
}

} // namespace distribuild::client


int main(int argc, char** argv) {
  // 环境
  setenv("LC_ALL", "en_US.utf8", true);

  // 日志
  if (argc == 1) {
      LOG_INFO("没有编译任务，退出...");
      return 0;
  }
  
  // 编译
  LOG_TRACE("开始编译");
  int rt = distribuild::client::Compile(argc, argv);
  LOG_TRACE("编译完成");

  // 退出
  return rt;
}
