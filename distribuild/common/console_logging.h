#pragma once
#include <fmt/format.h>
#include <stdarg.h>
#include <sys/time.h>

#if 0

namespace distribuild {

inline std::string GetNowTime() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t time_now = tv.tv_sec;
    struct tm time_info;
    localtime_r(&time_now, &time_info);
    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:02d}",
                        time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday, 
                        time_info.tm_hour, time_info.tm_min, time_info.tm_sec, tv.tv_usec / 10000);
}

template <typename... Args>
std::string FormatLogOpt(const std::string& format_str, Args&&... args) noexcept {
    std::string result;
	if (sizeof...(args) == 0) return format_str;
	result = fmt::vformat(format_str, fmt::make_format_args(args...));
    return result;
}

inline int min_log_level = 0;

} // namespace distribuild

#define DISTBU_FORMAT_LOG(format_str, ...) \
    distribuild::FormatLogOpt(format_str, __VA_ARGS__)

#define DISTBU_LOG(level_str, level, format_str, ...)               \
    if (level >= distribuild::min_log_level) {                      \
        fprintf(stderr, "[%s]\t[%s]\t[%s:%d\t%s]\t%s\n",            \
                distribuild::GetNowTime().c_str(), level_str,       \
                __FILE__, __LINE__, __func__,                       \
                distribuild::FormatLogOpt(format_str, ##__VA_ARGS__).c_str()); \
    }

#define LOG_DEBUG(...)  DISTBU_LOG("DEBUG", 0, __VA_ARGS__)
#define LOG_TRACE(...)  DISTBU_LOG("TRACE", 1, __VA_ARGS__)
#define LOG_INFO(...)   DISTBU_LOG("INFO",  2, __VA_ARGS__)
#define LOG_WARN(...)   DISTBU_LOG("WARN",  3, __VA_ARGS__)
#define LOG_ERROR(...)  DISTBU_LOG("ERROR", 4, __VA_ARGS__)
#define LOG_FATAL(...)                                              \
    do {                                                            \
        DISTBU_LOG("FATAL", 5, __VA_ARGS__);                        \
        abort();                                                    \
    } while(0)

#define DISTBU_CHECK_FORMAT(expr, ...)                   \
    do {                                                             \
        if (!(expr)) {                                               \
            std::string msg;                                         \
            msg = fmt::format("Check failed: [{}]. {}", #expr,       \
                distribuild::FormatLogOpt(__VA_ARGS__)); \
            LOG_FATAL("{}", msg);                                    \
        }                                                            \
    } while(0)

#define DISTBU_CHECK(expr) DISTBU_CHECK_FORMAT(expr, "", "")

#endif