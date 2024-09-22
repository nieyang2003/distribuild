## RunningTaskBookkeeper类
```std::unordered_map<std::string, std::vector<RunningTask>> running_tasks_;```记录节点正在运行的任务
其它是对上面的增删

## TaskDispatcher单例类
```next_task_id_```唯一task_id
记录所有节点和任务数以及RunningTaskBookkeeper

### 构造函数
设定启动任务的最小内存，启动`OnTimerExpiration`定时器

### WaitForStartingNewTask函数
找到有对应编译器的所有节点，从这些节点里找到空闲的机器，如果暂无则条件变量等待一会，最后从中挑选一个可用节点，返回唯一任务id和编译节点地址

### UnsafeGetServantsHasEnv函数
根据编译器环境、节点权限（非可用节点编译并发数为0）、版本找到所有节点并返回

### UnsafeGetFreeServants函数
从传进来的节点中挑选有剩余负载的节点并返回

### UnsafePickUpFreeServant函数
先查找是否有相同ip节点，否则查找是否有专用编译节点，最后找其它编译节点

### AvailableTasks函数
根据节点的内存与启动任务最小内存比较，不够返回最大任务数，否则计算当前任务数返回

### UnsafeClearZombies函数
遍历任务，如果is_zombie为true且正在运行的任务里没有此任务，则调用UnsafeFreeTask函数清除这些任务

### KeepTaskAlive函数
延长任务的过期时间

### FreeTask函数
加锁，调用UnsafeFreeTask函数

### KeepServantAlive函数
延长节点的过期时间，第一次则新增

### NotifyServantRunningTasks函数
更新节点上正在运行的任务

### UnsafeFreeTask
移除任务，更新对应节点

### OnTimerExpiration函数
清除过期节点、过期节点的任务，并将过期任务标记为僵尸任务

## SchedulerServiceImpl类
```std::deque<std::string> active_daemon_tokens_;```只有三个令牌：即将过期、正在使用、正在被部署

### HeartBeat函数
验证token、版本、超时，获得真实ip，更新相关数据

### GetConfig函数
返回active_daemon_tokens_[1]

### WaitForStaringTask函数
检查token，任务的最长等待时间是否合理
根据立即任务请求由TaskDispatcher分配一个节点返回，预取节点也分配一个节点

### KeepTaskAlive函数
检查token，任务的最长等待时间是否合理
TaskDispatcher->KeepTaskAlive

### FreeTask函数
### GetRunningTasks函数

### ActiveDaemonTokens函数
调用时轮换token，返回三个token