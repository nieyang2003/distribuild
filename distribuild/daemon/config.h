#pragma once
#include <optional>
#include <vector>
#include <string>
#include <gflags/gflags.h>

namespace distribuild::daemon {

DECLARE_string(servant_location);

DECLARE_string(scheduler_token);

DECLARE_string(cache_server_token);

DECLARE_string(scheduler_location);

DECLARE_string(cache_server_location);

DECLARE_string(compiler_dir_path);

DECLARE_string(min_memory_for_starting_new_task);

DECLARE_string(servant_priority);

DECLARE_int32(cpu_load_average_seconds);

DECLARE_int32(max_concurrency);

DECLARE_uint32(poor_cpu_cores_threshold);

DECLARE_bool(allow_core_dump);

DECLARE_int64(compilers_rescan_timer_intervals);

DECLARE_int64(heart_beat_timer_intervals);

DECLARE_uint64(chunk_size);

}