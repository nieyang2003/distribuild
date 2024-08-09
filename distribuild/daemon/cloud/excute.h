#pragma once

#include <string>

namespace distribuild::daemon::cloud {

/// @brief 执行命令
/// @param cmd 
/// @param nice_level nice值
/// @param stdin_fd 
/// @param stdout_fd 
/// @param stderr_fd 
/// @param group 
/// @return 
pid_t StartProgram(const std::string& cmd, int nice_level, int stdin_fd, int stdout_fd, int stderr_fd, bool in_group);

}