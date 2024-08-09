#include "common/logging.h"
#include "common/tools.h"
#include "client/common/utility.h"
#include "client/common/command.h"
#include "client/common/task_quota.h"
#include "common/io.h"
#include "client/cxx/compilition.h"
#include "client/cxx/compiler_args.h"
#include "client/cxx/rewrite_file.h"

namespace distribuild::client {

using namespace std::literals;

/// @brief 根据参数判断是否可以在远程执行
bool IsDistributable(const CompilerArgs& args) {
  if (!args.TryGet("-c")) {
	LOG_TRACE("非编译任务 '-c'"); // 重写后删去
	return false;
  }
  if (args.TryGet("-")) {
	LOG_TRACE("不支持从标准输入读取源码");
	return false;
  }
  if (args.GetFilenames().size() != 1) {
	LOG_TRACE("不支持多个文件");
	return false;
  }
  if (EndWith(args.GetFilenames()[0], ".s") || EndWith(args.GetFilenames()[0], ".S")) {
	LOG_TRACE("不支持编译汇编");
	return false;
  }
  // ! 更多选项...
  return true;
}

/// @brief 根据参数判断是否为轻量级任务
bool IsLightweight(const CompilerArgs& args) {
  static constexpr std::string_view kLightweightArgs[] = {"-dumpversion", "-dumpmachine", "-E"};
  // -dumpversion: 打印出 GCC 的版本号
  // -dumpmachine: 打印出目标机器的信息
  // -E: 只进行预处理

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
int Compile(int argc, const char** argv) {
  // distribuild g++ ...
  if (!EndWith(argv[0], "distribuild")) {
	LOG_WARN("程序名 {} 非distribuild", argv[0]);
  }

  CompilerArgs args(argc - 2, argv + 2);

  // 找到实际编译器绝对路径
  if (argv[1][0] == '/') {
	// distribuild /usr/bin/.../g++ 
	LOG_DEBUG("使用绝对路径 `{}`", argv[1]);
    args.SetCompiler(argv[1]); // 直接使用了绝对路径
  } else {
	// 找到绝对路径
    args.SetCompiler(FindExecutableInPath(GetBaseName(argv[1]), [](auto&& path) {
	    return !EndWith(path, "ccache") &&
               !EndWith(path, "distcc");
	  }));
  }
  LOG_TRACE("使用编译器: {}", args.GetCompiler());

  // 直接本地编译
  auto compile_on_native = [&](auto&& quota) {
    return CompileOnNativeUsingQuota(args.GetCompiler(), argv + 2, quota);
  };
  // 编译前后获得、释放配额防止超载
  auto compile_on_native_using_quota = [&] {
    return compile_on_native(AcquireTaskQuota(IsLightweight(args)));
  };

  // 是否可在云端执行
  if (!IsDistributable(args)) {
	LOG_TRACE("非远程执行任务，开始本地执行");
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
      && config::GetCacheControl() != CacheControl::Disallow) {  // 环境允许但重写结果不允许
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
	// 提交编译
    CompileResult compile_result = CompileOnCloud(args, std::move(*rewritten));
	// 获取结果与重试
	if (compile_result.exit_code < 0 || compile_result.exit_code == 127) { // 失败或编译器错误
	  if (auto quota = TryAcquireTaskQuota(false, 10s)) {
		LOG_INFO("云端编译失败 exit_code = {}，尝试本地编译。", compile_result.exit_code);
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

	// 云端编译失败
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
	fprintf(stdout, "%s", compile_result.std_out.c_str());
	fprintf(stderr, "%s", compile_result.std_err.c_str());
	WriteCompileResult(args.GetOutputFile(), compile_result.output_files);
	break;
  }

  return 0;
}

} // namespace distribuild::client


int main(int argc, const char** argv) {
  // 环境
  setenv("LC_ALL", "en_US.utf8", true);

  // TODO: 初始化

  // 参数
  if (argc == 1) {
      LOG_INFO("没有编译任务，退出...");
      return 0;
  }
  
  // 编译
  LOG_TRACE("开始编译");
  int rt = distribuild::client::Compile(argc, argv);
  LOG_TRACE("退出编译");

  // 退出
  return rt;
}
