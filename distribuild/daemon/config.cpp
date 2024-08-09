#include "daemon/config.h"

namespace distribuild::daemon {

DEFINE_string(servant_location, "127.0.0.1:10100", "daemon地址");

DEFINE_string(scheduler_token, "nieyang", "调度器token");

DEFINE_string(cache_server_token, "nieyang", "缓存节点token");

DEFINE_string(scheduler_location, "127.0.0.1:10005", "调度器地址");

DEFINE_string(cache_server_location, "127.0.0.1:10015", "缓存节点位置");

DEFINE_string(compiler_dir_path, "", "用户自定义编译器位置");

DEFINE_string(min_memory_for_starting_new_task, "2G", "允许接受新任务的最小内存");

DEFINE_string(servant_priority, "user", "守护进程节点种类");

DEFINE_int32(cpu_load_average_seconds, 0, "");

DEFINE_int32(max_concurrency, -1, "最大并发数，指定-1由程序自己设置");

DEFINE_uint32(poor_cpu_cores_threshold, 4, "小于该核心数的机器不接受编译任务");

DEFINE_bool(allow_core_dump, false, "是否打开core dump，默认不打开");

DEFINE_int64(compilers_rescan_timer_intervals, 60'000, "重新扫描编译器的定时器的时间间隔，默认60s");

DEFINE_int64(heart_beat_timer_intervals, 1'000, "心跳定时器时间间隔，默认1s");

DEFINE_uint64(chunk_size, 64 * 1024, "发送文件的分块大小");

}