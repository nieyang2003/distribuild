#pragma once

#include <string>
#include <vector>

#include "distribuild/client/cxx/compiler_args.h"
#include "distribuild/client/cxx/rewrite_file.h"

namespace distribuild::client {

struct CompileResult {
  int exit_code;
  std::string out_put;
  std::string error;
  // <文件名, 压缩字节流>
  std::vector<std::pair<std::string, std::string>> output_files;
};

std::optional<std::string> SubmitComileTask(const CompilerArgs& args, RewriteResult rewritten_source);

CompileResult WaitForCloudCompileResult(const CompilerArgs& args, const std::string task_id);

CompileResult CompileOnCloud(const CompilerArgs& args, RewriteResult rewritten_source);

} // namespace distribuild::client