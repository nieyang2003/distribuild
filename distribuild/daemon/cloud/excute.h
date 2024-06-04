#pragma once

#include <string>

namespace distribuild::daemon::cloud {

pid_t StartProgram(const std::string& cmd, int nice_level, int stdin_fd, int stdout_fd, int stderr_fd, int group);

}