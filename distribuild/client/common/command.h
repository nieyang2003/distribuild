#pragma once

#include "distribuild/client/common/task_quota.h"
#include "distribuild/client/common/rewritten_args.h"
#include "distribuild/client/common/out_stream.h"

#include <string>

namespace distribuild::client {

/// @brief 本地编译
int CompileOnNative(const std::string& program, const char** argv);

/// @brief 本地编译
int CompileOnNativeUsingQuota(const std::string& program, const char** argv, TaskQuota quato);

/// @brief 在子进程中执行命令并将结果返回到对应对象中
/// @param command 重写好的命令
/// @param extra_envs 额外环境
/// @param input 输入
/// @param standard_output 输出
/// @param standard_error 错误
/// @return 
int ExecuteCommand(const RewrittenArgs& command,
                   const std::initializer_list<std::string>& extra_envs,
                   const std::string& input,
				   OutStream* standard_output,
                   std::string* standard_error);

}