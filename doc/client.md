## 运行简图

```
distribuild gcc ...
```
运行过程（省略参数）
```
           main()
             |
             v
         Compile()-------------------------------+- 构造 CompilerArgs
                                                 |- 找到编译器绝对路径 --传给--> CompilerArgs
                                                 |- 检查参数判断能否继续 [quit]
       RewriteFile() ----------------------------+- 预处理源文件并压缩生成缓存键值
             |                                   |- 如果文件太小则直接本地编译 [quit]
             |-                                  |- while(true)
             |-                                  | 
                                                 | 
      CompileOnCloud() --------------------------| 
             |                                   |- 处理结果，重新提交等
             |-SubmitComileTask()                |- 写入文件
             |-WaitForCloudCompileResult()       |

```

## 基本流程
设置区域变量
检查是否有参数
检查程序名
获得编译器绝对路径
构造参数
检查是否可云端编译：-c只进行编译，-从标准输入获取源码，是否有多个文件或汇编任务
RewriteFile，重写源文件
设置编译选项
尝试云端编译
失败重试
成功写入结果


## CompilerArgs类
### 构造函数
逐个检查编译选项，如果选项后面紧跟一个参数则记录，否则记录文件，最后记录命令
### Rewrite函数
传入需要移除的参数和需要添加的参数，将编译器地址和vector<string>传入RewrittenArgs类构造函数并返回
### GetOutputFile函数
是否有‘-o’选项，返回后面的文件名，否则返回‘源文件名.o’
### TryGet函数
获得选项参数

## RewrittenArgs类
主要将参数连成字符串，去除'\n'多个空格等

## command.h/command.cpp
### ExecuteCommand函数
创建子进程，执行命令，并将0、1、2通过管道写入父进程并读取
### CompileOnNative函数
execvp执行并替换当前进程

## daemon_call.h/daemon_call.cpp
发送http1.1请求并接收响应，发送文件或json请求体

## out_stream.h/out_stream.cpp
zstd算法压缩源码，Blake3算法生成源码唯一

## task_quota.h/task_quota.cpp
向daemon获得配额

## rewrite_file.h/rewrite_file.cpp
### TryRewriteFileWithCommandLine函数
执行RewriteFile处理的预处理命令，写入输出流，压缩返回
### RewriteFile函数
‘-x’或文件后缀名判断语言类型
去除'-c' '-o'
‘-fno-working-directory’删除调试信息的绝对路径
‘-fdirectives-only’删除绝对路径
调用TryRewriteFileWithCommandLine函数，返回编译结果

## compilition.h/compilition.cpp
### SubmitComileTask函数
将编译任务相关数据发送http请求到daemon，如果没有执行则发送编译器的信息重试一次
不断发送wait_for_cxx_task请求保持任务执行直到返回结果，对结果进行解压缩
### WaitForCloudCompileResult函数

### CompileOnCloud函数
调用SubmitComileTask函数和WaitForCloudCompileResult函数