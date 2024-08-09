#pragma once
#include "client/cxx/compiler_args.h"
#include "client/cxx/rewrite_file.h"
#include <string>
#include <vector>

namespace distribuild::client {

/// @brief 编译结果
struct CompileResult {
  int exit_code;
  std::string std_out;
  std::string std_err;
  // <扩展名, 压缩字节流>
  std::vector<std::pair<std::string, std::string>> output_files;
};

/// @brief 提交编译任务
/// @param args 编译参数
/// @param rewritten_source 重写结果
/// @return task_id
std::optional<uint64_t> SubmitComileTask(const CompilerArgs& args, RewriteResult rewritten_source);

/// @brief 
/// @param args 
/// @param task_id 
/// @return 
CompileResult WaitForCloudCompileResult(const CompilerArgs& args, const uint64_t task_id);

/// @brief 在云端编译
/// @param args 
/// @param rewritten_source 
/// @return 
CompileResult CompileOnCloud(const CompilerArgs& args, RewriteResult rewritten_source);

} // namespace distribuild::client