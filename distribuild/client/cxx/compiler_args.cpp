#include "compiler_args.h"
#include "logging.h"

#include <unordered_set>
#include <string_view>

#include "distribuild/common/string.h"

namespace distribuild::client {

namespace {

const std::unordered_set<std::string_view> kOneValueArguments = {
    "-o",
    "-x",

    "-dyld-prefix",
    "-gcc-toolchain",
    "--param",
    "--sysroot",
    "--system-header-prefix",
    "-target",
    "--assert",
    "--allowable_client",
    "-arch",
    "-arch_only",
    "-arcmt-migrate-report-output",
    "--prefix",
    "-bundle_loader",
    "-dependency-dot",
    "-dependency-file",
    "-dylib_file",
    "-exported_symbols_list",
    "--bootclasspath",
    "--CLASSPATH",
    "--classpath",
    "--resource",
    "--encoding",
    "--extdirs",
    "-filelist",
    "-fmodule-implementation-of",
    "-fmodule-name",
    "-fmodules-user-build-path",
    "-fnew-alignment",
    "-force_load",
    "--output-class-directory",
    "-framework",
    "-frewrite-map-file",
    "-ftrapv-handler",
    "-image_base",
    "-init",
    "-install_name",
    "-lazy_framework",
    "-lazy_library",
    "-meabi",
    "-mhwdiv",
    "-mllvm",
    "-module-dependency-dir",
    "-mthread-model",
    "-multiply_defined",
    "-multiply_defined_unused",
    "-rpath",
    "--rtlib",
    "-seg_addr_table",
    "-seg_addr_table_filename",
    "-segs_read_only_addr",
    "-segs_read_write_addr",
    "-serialize-diagnostics",
    "--serialize-diagnostics",
    "-std",
    "--stdlib",
    "--force-link",
    "-umbrella",
    "-unexported_symbols_list",
    "-weak_library",
    "-weak_reference_mismatches",
    "-B",
    "-D",
    "-U",
    "-I",
    "-i",
    "--include-directory",
    "-L",
    "-l",
    "--library-directory",
    "-MF",
    "-MT",
    "-MQ",
    "-cxx-isystem",
    "-c-isystem",
    "-idirafter",
    "--include-directory-after",
    "-iframework",
    "-iframeworkwithsysroot",
    "-imacros",
    "-imultilib",
    "-iprefix",
    "--include-prefix",
    "-iquote",
    "-include",
    "-include-pch",
    "-isysroot",
    "-isystem",
    "-isystem-after",
    "-ivfsoverlay",
    "-iwithprefix",
    "--include-with-prefix",
    "--include-with-prefix-after",
    "-iwithprefixbefore",
    "--include-with-prefix-before",
    "-iwithsysroot"};

/// @brief 转义字符串中的特殊字符，以便在命令行参数中安全使用
std::string EscapeCommandArgument(const std::string_view& str) {
  std::string result;
  for (size_t i = 0; i < str.size(); ++i) {
    switch (str[i]) {
      case '\a':
        result += "\\a";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      case '\v':
        result += "\\v";
        break;
      case ' ':
      case '>':
      case '<':
      case '!':
      case '"':
      case '#':
      case '$':
      case '&':
      case '(':
      case ')':
      case '*':
      case ',':
      case ':':
      case ';':
      case '?':
      case '@':
      case '[':
      case '\\':
      case ']':
      case '`':
      case '{':
      case '}':
        result += '\\';
        [[fallthrough]];
      default:
        result += str[i];
        break;
    }
  }
  return result;
}

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
	DISTBU_CHECK(GetFilenames().size() == 1);
	std::string_view filename = GetFilenames()[0];
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