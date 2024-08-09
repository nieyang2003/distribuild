#include <sys/syscall.h>
#include <unistd.h>
#include "daemon/cloud/excute.h"
#include "common/logging.h"

namespace distribuild::daemon::cloud {

pid_t StartProgram(const std::string& cmd, int nice_level, int stdin_fd, int stdout_fd, int stderr_fd, bool in_group) {
  int pid = fork();
  DISTBU_CHECK_FORMAT(pid >= 0, "创建子进程失败");
  if (pid == 0) { // 子进程
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
      DISTBU_CHECK_FORMAT(nice(nice_level) != -1, "设置nice值失败：{}", nice_level);
    }

    // 设置进程组
	if (in_group) {
      setpgid(0, 0);
    }

	// 改变工作目录
	DISTBU_CHECK(chdir("/") == 0);

	// 执行命令
	const char* const argvs[] = {"sh", "-c", cmd.c_str(), nullptr};
	syscall(SYS_execve, "/bin/sh", argvs, environ);
	_exit(127);
  }

  LOG_DEBUG("子进程 pid = {}", pid);
  return pid;
}

}