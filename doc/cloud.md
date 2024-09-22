[grpcpp api](https://grpc.github.io/grpc/cpp/index.html)

## TempFile类
### 构造函数
mkostemps(temp_name, 0, O_CLOEXEC);创建临时文件
readlink获取文件名

### Write函数
写入字符串

### ReadAll函数
读取所有内容到字符串中

## TempDir类

### 构造函数
创建目录
"{}/distribuild_{}_{}"

### ReadAll函数
读取目录下所有 文件名-二进制内容

### Clear函数
清除目录

## excute.h/excute.cpp
启动子进程，重定向0、1、2到传入的fd
关闭其它描述符
设置进程优先级
设置进程组
改变工作目录到“/”
// 执行命令
const char* const argvs[] = {"sh", "-c", cmd.c_str(), nullptr};
syscall(SYS_execve, "/bin/sh", argvs, environ);
_exit(127);

## Compilers类

### OnTimerRescan函数
定时扫描，加写锁
在PATH中查找编译器
在用户配置的路径中查找编译器
更新编译器

### GetPathHasCompiler函数
检查路径文件是否存在
检查编译器是否对当前用户、组或其他用户可执行
返回绝对路径

### SearchCompiler函数
搜寻指定目录下的gcc、g++编译器，调用GetPathHasCompiler函数获得绝对路径保持返回

### TryGetFileDigest函数
读取可执行文件二进制数据hash出一个字符串并返回

### AddEnvTo函数
添加到容器中并利用digest去重

## CxxCompileTask : public CompileTask

### 构造函数
创建TempDir作为工作目录

### Prepare函数
查找编译器是否存在
检查压缩类型
解压源码
解析请求
如果请求填充缓存则检查是否有时间相关宏
最后由source_digest_创建工作目录下的相关目录
改写cmdline："{} {} -o {}/{}/output.o"

### PrepareCachePocoTask任务
写入缓存之间检查是否有时间相关宏

### GetOutput函数
读取工作目录下的所有文件
截取文件后缀名并写入额外信息

### OnCompleted函数
传入标准输出和标准错误
GetOutput获得文件
压缩并打包
尝试异步写入缓存

## Executor类

### 构造函数
加载最小物理内存配置
配置最大任务数
启动子进程等待线程和定时器

### TryQueueTask函数
生成task_id
放入任务表中
创建临时输入、输出、错误文件
启动任务，重定向fd

### TryStartingNewTaskUnsafe函数
判断是否任务数上线和内存，返回任务id

### TryAddTaskRef函数

### WaitForTask函数
根据task_id返回对应任务

### FreeTask函数
减少应用计数，为0则删除
KillTask

### KillExpiredTasks函数
删除传入的过期的task_id对应的任务，并kill对应的进程

### KillTask
kill -9 pid

### OnTimerClean函数
定时清除编译完成且超时1s的任务

### OnExitCallback函数
### OnExitTask异步任务
更新状态，读取文件内容，move到OnCompleted函数中

### WaiterProc函数
等待一个子进程，启动OnExitTask异步任务

## DaemonServiceImpl类

### 构造函数
启动心跳定时器

### OnTimerHeartbeat函数
发送本地信息心跳包，返回心跳响应
由响应更新token
由响应调用 Executor::Instance()->KillExpiredTasks

### QueueCxxTask RPC函数
接收编译任务和文件
task->Prepare
Executor::Instance()->TryQueueTask

### AddTaskRef函数

### WaitForTask函数
等待一段时间：Executor::Instance()->WaitForTask
获得输出发回响应