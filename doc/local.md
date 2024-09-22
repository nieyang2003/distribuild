## TaskRunKeeper类
### OnTimerRefresh函数
```scheduler_stub_->GetRunningTasks```
更新调度器当前正在运行的任务，记录对应的任务id和编译节点地址

### TryFindTask函数
由task_digest找到节点地址

## TaskMonitor类
### 构造函数
加载配置的最大并发数，轻量任务并发数为1.5倍

### WaitForNewTask函数
阻塞一段时间等待任务释放，判断是否重复添加，增加任务数记录

### DropTask函数
删除任务

### OnTimerCheckAliveProc函数
定时检查每个任务是否存活，不存在则删除

### IsAliveProc函数
format("/proc/{}/status", pid)
判断程序的State是否是僵尸或死亡

## TaskGrantKeeper类
std::unordered_map<std::string, std::unique_ptr<EnvGrantKeeper>> keepers_;
编译器信息所对应的EnvGrantKeeper

### 构造函数
创建调度器任务stub

### Get函数
查询对应编译器是否存在，不存在则新建并启动EnvGrantKeeper的定时器任务
清理过期的节点
有剩余节点则弹出返回，没有通知need_more_cv去申请节点并等待available_cv

### Free函数
由grant_id/唯一task_id请求调度器释放

### GrantFetcherProc函数
循环直到退出
不断等待need_more_cv，满足后发起请求要调度器分配编译节点
成功后更新EnvGrantKeeper的节点队列并通知available_cv

## FileCache类
文件路径、大小、修改时间、hash属性作为缓存条目缓存起来

## ConfigKeeper类
定时向调度器刷新token

## TaskDispatcher类

### QueueTask函数
生成task_desc并启动一个PerformTask异步任务

### WaitForTask

### PerformTask
查缓存查看是否有结果
查看任务是否正在运行
的确不存在，启动新任务StartNewServantTask

### StartNewServantTask函数
验证更新任务信息
根据分配的节点创建对应的stub
调用不同任务的虚函数StartTask
再WaitServantTask
再FreeServantTask

### WaitServantTask函数
重试多次，调用编译节点WaitForTask获取编译结果

### FreeServantTask函数
调用编译节点FreeServantTask rpc函数

### TryGetExistedResult函数
创建到对应编译节点的连接
调用AddTaskRef函数
失败则表明此任务不存在
否则同样wait、free

### OnTimerTimeoutAbort定时器函数
检查程序启动时间超时，设置aborted位

### OnTimerKeepAlive定时器函数
对运行中的程序尝试keep-alive，防止被编译节点杀死
检查最后keep-alive的时间点是否超时，设置aborted位
一次性向调度器KeepTaskAlive多个任务

### OnTimerKilledAbort函数
检查没有aborted但却不存在的进程，清理相关数据

### OnTimerClear
遍历任务，检查aborted位并清除数据


## HttpServiceImpl类

### 构造函数
设置所有http接口

### AcquireQuota
获得任务配额：TaskMonitor::Instance()->WaitForNewTask(...)

### ReleaseQuota
释放配额：TaskMonitor::Instance()->DropTask(...)

### SetFileDigest
设置编译器缓存条目

### SubmitCxxTask

### WaitForTask

### AskToLeave

## CxxDistTask : public DistTask
### Prepare函数
查询编译器是否存在并更新相关信息，在收到http请求后交给TaskDispatcher之前执行

### StartTask函数
向编译节点QueueCxxTask发送文件，在TaskDispatcher::StartNewServantTask中被调用