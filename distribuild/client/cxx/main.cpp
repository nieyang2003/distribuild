#include "distribuild/common/logging.h"
#include "compiler_args.h"
#include "rewrite_file.h"
#include "common/string.h"

namespace distribuild::client {

int Compile(int argc, char* argv[]) {
  bool use_this = EndWith(argv[0], "distribuild");
  int skip = use_this ? 2 : 1;

  CompilerArgs args(argc - skip, argv + skip);
  if (use_this && argv[1][0] == '/') {
    args.SetCompiler(argv[1]); // 使用了绝对路径
  } else {
    args.SetCompiler();
  }
  LOG_TRACE("使用编译器: {}", args.GetCompiler());

  auto run_native = [&] {
    // return RunLocal();
  };
  auto run_cloud = [&] {
    
  };

  // 如果无法在云端执行，则放弃
  if () {
    return run_native();
  }

  auto rewritten = RewriteFile(args);
  if (!rewritten) {
	LOG_INFO("参数错误，无法重写文件，开始本地执行: {}", args.RebuiltArg());
    return run_native();
  }

  if () {
	LOG_WARN("存在不可缓存的文件。参数: {}", args.RebuiltArg());
  }

  if () {
	LOG_TRACE("预处理文件太小，本地编译速度可能更快");
	return run_native();
  }

  while (true) {
    auto&& [rt, ...] CompileOnCloud(args, std::move(*rewritten));
	if (rt < 0 || rt == 127) {

	}
	if (rt != 0) {
	  LOG_TRACE("在云端编译失败");
	  return run_native();
	}

	LOG_TRACE("云端编译成功");
	WriteResult();
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

  LOG_TRACE("开始编译");
  int rt = distribuild::client::Compile(argc, argv);
  LOG_TRACE("编译完成");
  return rt;
}
