#pragma once
#include <spdlog/spdlog.h>
#include <gflags/gflags.h>
#include <spdlog/sinks/daily_file_sink.h>
#ifdef SPDLOG_ACTIVE_LEVEL
#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#define LOG_DEBUG  spdlog::debug
#define LOG_TRACE  spdlog::trace
#define LOG_INFO   spdlog::info
#define LOG_WARN   spdlog::warn
#define LOG_ERROR  spdlog::error
#define LOG_FATAL(...)                            \
    do {                                          \
        spdlog::critical(__VA_ARGS__);            \
        abort();                                  \
    } while(0)

#define DISTBU_CHECK_FORMAT(expr, fmt_str, ...)                      \
    do {                                                             \
        if (!(expr)) {                                               \
            spdlog::critical("Check failed: [{}]. " fmt_str, #expr, ##__VA_ARGS__); \
        }                                                            \
    } while(0)


#define DISTBU_CHECK(expr) DISTBU_CHECK_FORMAT(expr, "", "")