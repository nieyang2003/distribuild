#include "compiler_args.h"
#include "logging.h"

#include <unordered_set>
#include <string_view>

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

}  // namespace

CompilerArgs::CompilerArgs(int argc, const char** argv) {
  DISTBU_CHECK(argc >= 1); // 参数在传入前已经处理
  
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
}

}