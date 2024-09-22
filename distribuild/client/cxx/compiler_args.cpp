#include "client/cxx/compiler_args.h"
#include "common/spdlogging.h"
#include "common/tools.h"
#include <unordered_set>
#include <string_view>

namespace distribuild::client {

namespace {

/// @brief 后面紧跟参数的选项
const std::unordered_set<std::string_view> kOneValueArguments = {
    "-o",  // 指定输出文件的名称
    "-x",  // 明确指定输入文件的语言类型 (如 c、c++、assembler)
    
    "-dyld-prefix",  // 设置动态链接库前缀 (特定平台使用)
    "-gcc-toolchain",  // 指定用于编译的 GCC 工具链路径
    "--param",  // 设置编译器参数 (例如 `--param max-inline-insns-single=100`)
    "--sysroot",  // 设置编译过程中使用的系统根目录
    "--system-header-prefix",  // 指定系统头文件的路径前缀
    "-target",  // 为编译器指定目标架构 (如 `-target x86_64-linux-gnu`)
    "--assert",  // 启用或禁用某些编译器断言
    "--allowable_client",  // 指定可以连接到此编译器客户端的名称
    "-arch",  // 指定目标架构 (如 `-arch x86_64`)
    "-arch_only",  // 仅为指定的架构生成代码
    "-arcmt-migrate-report-output",  // 为 ARC (Automatic Reference Counting) 迁移生成报告
    "--prefix",  // 设置前缀路径，通常用于指定安装路径
    "-bundle_loader",  // 指定与 bundle 关联的可执行文件
    "-dependency-dot",  // 生成以 .dot 格式表示的依赖图文件
    "-dependency-file",  // 指定依赖文件的输出文件名
    "-dylib_file",  // 动态库文件的映射 (特定平台使用)
    "-exported_symbols_list",  // 指定导出符号列表文件
    "--bootclasspath",  // 指定 Java 虚拟机的引导类路径
    "--CLASSPATH",  // 设置 Java 的 CLASSPATH 环境变量
    "--classpath",  // 同上，用于指定 Java 的类路径
    "--resource",  // 设置资源路径
    "--encoding",  // 指定文件的编码方式 (如 `--encoding utf-8`)
    "--extdirs",  // 指定扩展目录路径
    "-filelist",  // 指定包含文件列表的文件 (通常用于链接)
    "-fmodule-implementation-of",  // 为模块指定实现
    "-fmodule-name",  // 指定模块名称
    "-fmodules-user-build-path",  // 设置模块构建路径
    "-fnew-alignment",  // 指定新对象的内存对齐方式
    "-force_load",  // 强制加载指定的动态库或静态库
    "--output-class-directory",  // 设置 Java 类文件的输出目录
    "-framework",  // 指定 macOS 下的框架
    "-frewrite-map-file",  // 设定重写规则的映射文件
    "-ftrapv-handler",  // 设置整型溢出处理函数
    "-image_base",  // 设置动态库的基地址
    "-init",  // 指定库的初始化函数
    "-install_name",  // 设置安装时动态库的路径 (macOS 使用)
    "-lazy_framework",  // 延迟加载指定的 macOS 框架
    "-lazy_library",  // 延迟加载指定的库
    "-meabi",  // 指定目标的 EABI 版本 (用于嵌入式系统)
    "-mhwdiv",  // 指定目标硬件除法支持 (ARM 架构使用)
    "-mllvm",  // 传递特定 LLVM 优化选项
    "-module-dependency-dir",  // 设置模块依赖路径
    "-mthread-model",  // 指定线程模型 (如 single、posix)
    "-multiply_defined",  // 指定如何处理多个定义符号
    "-multiply_defined_unused",  // 处理未使用的多重定义
    "-rpath",  // 设置运行时库搜索路径
    "--rtlib",  // 指定运行时库 (如 `--rtlib=libgcc` 或 `--rtlib=compiler-rt`)
    "-seg_addr_table",  // 设置段地址表
    "-seg_addr_table_filename",  // 设置段地址表文件名
    "-segs_read_only_addr",  // 设置只读段的起始地址
    "-segs_read_write_addr",  // 设置可读写段的起始地址
    "-serialize-diagnostics",  // 序列化诊断信息
    "--serialize-diagnostics",  // 同上
    "-std",  // 指定标准版本 (如 `-std=c++11`)
    "--stdlib",  // 指定 C++ 标准库实现 (如 `--stdlib=libc++`)
    "--force-link",  // 强制链接指定的库
    "-umbrella",  // 设置 macOS 下 umbrella 框架的路径
    "-unexported_symbols_list",  // 指定不导出的符号列表文件
    "-weak_library",  // 允许弱链接的库
    "-weak_reference_mismatches",  // 指定如何处理弱引用不匹配的情况
    "-B",  // 指定编译器或链接器的路径前缀
    "-D",  // 定义预处理宏 (如 `-DDEBUG`)
    "-U",  // 取消定义预处理宏 (如 `-UDEBUG`)
    "-I",  // 指定头文件搜索路径
    "-i",  // 同上，用于包括目录
    "--include-directory",  // 指定包含目录路径
    "-L",  // 指定库文件搜索路径
    "-l",  // 指定链接时的库文件
    "--library-directory",  // 同上，用于指定库目录
    "-MF",  // 指定依赖文件
    "-MT",  // 指定目标名称
    "-MQ",  // 生成目标的名称（带引号）
    "-cxx-isystem",  // 指定 C++ 头文件的系统路径
    "-c-isystem",  // 指定 C 头文件的系统路径
    "-idirafter",  // 指定在搜索头文件路径列表末尾的目录
    "--include-directory-after",  // 同上
    "-iframework",  // 指定 macOS 下的框架搜索路径
    "-iframeworkwithsysroot",  // 使用 sysroot 指定 macOS 框架路径
    "-imacros",  // 指定预处理时包含的宏文件
    "-imultilib",  // 指定多库路径 (multilib)
    "-iprefix",  // 指定包含文件的前缀路径
    "--include-prefix",  // 同上
    "-iquote",  // 指定在搜索本地头文件时使用的路径
    "-include",  // 强制编译器在处理源文件前包含指定头文件
    "-include-pch",  // 使用预编译头文件
    "-isysroot",  // 指定系统根路径
    "-isystem",  // 指定系统头文件的搜索路径
    "-isystem-after",  // 在系统路径之后添加一个头文件搜索路径
    "-ivfsoverlay",  // 指定虚拟文件系统覆盖
    "-iwithprefix",  // 添加带前缀的包含路径
    "--include-with-prefix",  // 同上
    "--include-with-prefix-after",  // 添加前缀的路径，且在其他路径之后
    "-iwithprefixbefore",  // 在其他路径之前添加带前缀的包含路径
    "--include-with-prefix-before",  // 同上
    "-iwithsysroot"  // 指定带 sysroot 的包含路径
};

}  // namespace

CompilerArgs::CompilerArgs(int argc, const char** argv) {
  DISTBU_CHECK(argc >= 1); // 参数在传入前已经处理
  
  // 编译选项与文件
  for (int i = 0; i < argc; ++i) {
	if (kOneValueArguments.count(argv[i])) {
	  args_.emplace_back(argv[i], OptionArgs(&argv[i + 1], 1));
	  ++i;
	} else {
	  if (argv[i][0] == '-') { // 选项
		args_.emplace_back(argv[i], OptionArgs());
	  } else { // 文件名
		filenames_.push_back(argv[i]);
	  }
	}
  }
  
  // 记录原命令
  for (int i = 0; i != argc; ++i) {
    rebuilt_arg_ += EscapeCommandArgument(argv[i]) + " ";
  }
  rebuilt_arg_.pop_back();  // 弹出多余空格
}

const CompilerArgs::OptionArgs* CompilerArgs::TryGet(const std::string_view& key) const {
  for (auto&& [k, v] : args_) {
	if (k == key) {
	  return &v;
	}
  }
  return nullptr;
}

std::string CompilerArgs::GetOutputFile() const {
  if (auto opt = TryGet("-o")) {
	return opt->front();
  } else {
	DISTBU_CHECK(filenames_.size() == 1);
	std::string_view filename = filenames_[0];
    auto path = filename.substr(0, filename.find_last_of('.'));
    if (auto pos = path.find_last_of('/')) {
      path = path.substr(pos + 1);
    }
    return std::string(path) + ".o";
  }
}

RewrittenArgs CompilerArgs::Rewrite(
    const std::unordered_set<std::string_view>& remove,
    const std::vector<std::string_view>& remove_prefix,
    const std::initializer_list<std::string_view>& add, bool keep_filenames) const {
  std::vector<std::string> result;
  for (auto&& [k, v] : args_) {
	// 完全匹配筛选
	if (remove.count(k)) continue;
    // 前缀匹配筛选
	bool skip = false;
	for (auto&& e : remove_prefix) {
	  if (StartWith(k, e)) {
		skip = true;
		break;
	  }
	}
	if (skip) continue;
    // 添加
	result.push_back(k);
	for (auto&& e : v) {
		result.push_back(e);
	}
  }
  // 添加
  for (auto&& e : add) {
	result.push_back(std::string(e));
  }
  // 文件
  if (keep_filenames) {
	for (auto&& e : filenames_) {
      result.push_back(e);
    }
  }
  return RewrittenArgs(compiler_, result);
}

}