#include "excute.h"

#include "distribuild/common/logging.h"

namespace distribuild::daemon::cloud {

pid_t StartProgram(const std::string& cmd, int nice_level, int stdin_fd, int stdout_fd, int stderr_fd, int group) {
  LOG_INFO("开始执行命令: {}", cmd);
  int pid = fork();
  DISTBU_CHECK(pid >= 0, "无法创建子进程");
  if (pid == 0) {
	// 重定向
	dup2(stdin_fd,  STDIN_FILENO);
    dup2(stdout_fd, STDOUT_FILENO);
    dup2(stderr_fd, STDERR_FILENO);
    lseek(STDIN_FILENO, 0, SEEK_SET); // 重置文件位置。
    close(stdin_fd);
    close(stdout_fd);
    close(stderr_fd);

    // 关闭其它描述符
	for (int i = 3; i != 9999; ++i) {
      (void)close(i);
    }

    // 优先级
	if (nice_level) {
      DISTBU_CHECK(nice(nice_level) != -1, "Failed to apply nice value [{}].", nice_level);
    }

    // 设置进程组
	if (group) {
      setpgid(0, 0);
    }

	// 改变工作目录
	DISTBU_CHECK(chdir("/") == 0);

	// 执行命令
	const char* const argvs[] = {"sh", "-c", cmd.c_str(), nullptr};
	syscall(SYS_execve, "/bin/sh", argvs, environ);
	_exit(127);
  }

  return pid;
}

}