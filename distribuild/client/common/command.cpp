#include "client/common/command.h"
#include "common/logging.h"
#include "common/io.h"
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

namespace distribuild::client {

namespace {

/// @brief 子进程信息
struct ProgramStartupInfo {
  int pid;
  int stdin_writer;
  int stdout_reader;
  int stderr_reader;
};

/// @brief 创建管道
std::pair<int, int> CreatePipe() {
  int fd[2];
  DISTBU_CHECK(pipe2(fd, 0) == 0);
  DISTBU_CHECK(fcntl(fd[1], F_SETPIPE_SZ, 128 * 1024) > 0);
  return {fd[0], fd[1]};
}

/// @brief 返回以空指针为数组结尾的argv数组作为程序运行环境
std::vector<const char*> BuildArguments(const RewrittenArgs& command) {
  std::vector<const char*> result;
  result.push_back(command.GetProgram().c_str());
  for (auto&& e : command.GetArgs()) {
    result.push_back(e.c_str());
  }
  result.push_back(nullptr);
  return result;
}

/// @brief 创建一个进程并将标准输入标准输出和标准错误连接到父进程
/// @param command 
/// @param extra_envs 
/// @return 
ProgramStartupInfo StartProgram(const RewrittenArgs& command, const std::initializer_list<std::string>& extra_envs) {
  // 管道
  int stdin_writer, stdout_reader, stderr_reader;  // 父进程使用（写stdin，收stdout，收stderr）
  int stdin_reader, stdout_writer, stderr_writer;  // 子进程使用（收stdin，写stdout，写stderr）
  std::tie(stdin_reader, stdin_writer)   = CreatePipe();
  std::tie(stdout_reader, stdout_writer) = CreatePipe();
  std::tie(stderr_reader, stderr_writer) = CreatePipe();
  
  // 设置环境
  char** envs = environ;
  std::vector<char*> envs_storage;
  if (extra_envs.size()) {
	// 设置原有环境
    auto ptr = environ;
    while (*ptr) {
      envs_storage.push_back(*ptr++);
    }
	// 设置传入的额外环境
    for (auto&& e : extra_envs) {
      envs_storage.push_back(const_cast<char*>(e.c_str()));
    }
    envs_storage.push_back(0);
    envs = envs_storage.data();
  }
  auto argvs = BuildArguments(command);

  // 连接父子进程
  int pid = vfork();
  DISTBU_CHECK_FORMAT(pid >= 0, "创建子进程失败");
  if (pid == 0) { // 子进程
    dup2(stdin_reader,   STDIN_FILENO);
	dup2(stdout_writer, STDOUT_FILENO);
    dup2(stderr_writer, STDERR_FILENO);
	close(stdin_reader);
    close(stdout_writer);
    close(stderr_writer);
	close(stdin_writer);
    close(stdout_reader);
    close(stderr_reader);
    syscall(SYS_execve, command.GetProgram().c_str(), argvs.data(), envs); // 执行命令
	_exit(127);
  }

  close(stdin_reader);
  close(stdout_writer);
  close(stderr_writer);

  SetNonblocking(stdin_writer);
  SetNonblocking(stdout_reader);
  SetNonblocking(stderr_reader);

  // 返回父进程信息
  return ProgramStartupInfo{.pid = pid,
                            .stdin_writer = stdin_writer, 
                            .stdout_reader = stdout_reader,
							.stderr_reader = stderr_reader};
}

/// @brief 获取子进程退出状态码
int GetChildExitCode(int pid) {
  int status;
  while (true) {
	pid_t result = waitpid(pid, &status, 0); // 阻塞等待
	if (result == -1) {
	  if (errno == EINTR) {  // 系统调用被中断则继续等待
		continue;
	  }
	  DISTBU_CHECK_FORMAT(false, "等待子进程失败");
	}
	break;
  }
  DISTBU_CHECK_FORMAT(WIFEXITED(status), "子进程错误退出 status = {}", status); // 
  return WEXITSTATUS(status); // 提取子进程的退出状态码并返回
}

/// @brief 处理父子进程管道 父方的标准IO事件
/// @param pinfo 父进程信息
/// @param input 输入流，输入给子进程
/// @param standard_output 子进程标准输出的目的地，调用Write接口写入不同对象中
/// @param standard_error 子进程标准错误目的地
void HandleProgramIO(const ProgramStartupInfo& pinfo, const std::string& input,
                     OutStream* standard_output, std::string* standard_error) {
  thread_local char io_buffer[128 * 1024]; // 128k
  // 统计数据
  std::size_t stdin_bytes = 0, stdout_bytes = 0;
  bool in_done = false, out_done = false, err_done = false;
  
  // 事件循环
  while (!in_done || !out_done || !err_done) {
	// 注册事件
    pollfd fds[3]{};
	int nfds = 0;

	if (!in_done) {
	  fds[nfds].fd = pinfo.stdin_writer;
	  fds[nfds++].events = POLLOUT;
	}
	if (!out_done) {
	  fds[nfds].fd = pinfo.stdout_reader;
	  fds[nfds++].events = POLLIN;
	}
	if (!err_done) {
	  fds[nfds].fd = pinfo.stderr_reader;
	  fds[nfds++].events = POLLIN;
	}

    // 等待事件
	int events = poll(fds, nfds, -1);
	if (events < 0) {
		if (errno == EINTR) continue;
		LOG_FATAL("poll返回错误: {}", events);
	}

	// 处理事件
	for (int i = 0; i < nfds; ++i) {
	  if (fds[i].revents == 0) continue;
      
	  if (fds[i].fd == pinfo.stdin_writer) {
		// 子进程与父进程管道连接
		// 子进程读取标准输入

		auto bytes = WriteTo(pinfo.stdin_writer, input, stdin_bytes);
		if (bytes >= 0) {
		  stdin_bytes += bytes;
		  if (stdin_bytes == input.size()) {
			in_done = true;
			DISTBU_CHECK_FORMAT(close(pinfo.stdin_writer) == 0, "close错误");
		  }
		} else {
		  LOG_WARN("子进程关闭标准输入");
		  in_done = true;
		  DISTBU_CHECK(close(pinfo.stdin_writer) == 0);
		}
	  } else if (fds[i].fd == pinfo.stdout_reader) {
		// 子进程与父进程管道连接
		// 子进程向标准输出写

        auto bytes = ReadTo(pinfo.stdout_reader, io_buffer, sizeof(io_buffer));
		// 接收到buffer再写入流中
		if (bytes > 0) {
          standard_output->Write(io_buffer, bytes);
		  stdout_bytes += bytes;
		} else {
		  DISTBU_CHECK(bytes == 0);
		  out_done = true;
          DISTBU_CHECK(close(pinfo.stdout_reader) == 0);
		}
	  } else if (fds[i].fd == pinfo.stderr_reader) {
		// 子进程与父进程管道连接
		// 子进程向标准错误写

        auto bytes = ReadTo(pinfo.stderr_reader, io_buffer, sizeof(io_buffer));
		if (bytes > 0) {
          standard_error->append(io_buffer, bytes);
		} else {
		  DISTBU_CHECK(bytes == 0);
		  err_done = true;
          DISTBU_CHECK(close(pinfo.stderr_reader) == 0);
		}
	  } else {
		LOG_WARN("未知fd = {}", fds[i].fd);
	  }
	}
  }
  
  LOG_DEBUG("标准输入写入{}字节，标准输出{}字节，标准错误{}字节",
            stdin_bytes, stdout_bytes, standard_error->size());
}

} // namespace

// --------------------------------------------------------------------------- //

int CompileOnNative(const std::string& program, const char** argv) {
  LOG_DEBUG("开始本机直接编译");

  // 构造命令
  std::vector<const char*> argvs;
  argvs.push_back(program.c_str());
  while (*argv) {
    argvs.push_back(*argv++);
  }
  argvs.push_back(nullptr);

  int pid = fork();
  DISTBU_CHECK_FORMAT(pid >= 0, "创建子进程失败");

  if (pid == 0) { // child
    execvp(argvs[0], const_cast<char* const *>(argvs.data()));
	_exit(127);
  }

  return GetChildExitCode(pid);;
}

int CompileOnNativeUsingQuota(const std::string& program, const char** argv, [[maybe_unused]] TaskQuota quato) {
  return CompileOnNative(program, argv);
  // call the deleter function of quato
}

int ExecuteCommand(const RewrittenArgs& command,
                   const std::initializer_list<std::string>& extra_envs,
                   const std::string& input,
				   OutStream* standard_output,
                   std::string* standard_error) {
  LOG_DEBUG("开始预处理：`{}`", command.ToString());
  ProgramStartupInfo pinfo = StartProgram(command, extra_envs);
  HandleProgramIO(pinfo, input, standard_output, standard_error);
  auto exit_code = GetChildExitCode(pinfo.pid);
  LOG_DEBUG("预处理完毕，exit_code = {}", exit_code);
  return exit_code;
}

}