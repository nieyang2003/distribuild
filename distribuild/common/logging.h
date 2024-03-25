/**
 * @file logging.h
 * @author nieyang (nieyang2003@qq.com)
 * @brief 日志相关宏
 * @version 0.1
 * @date 2024-03-25
 * 
 * 
 */

#ifndef __DISTBU_COMMON_LOGGING_H__
#define __DISTBU_COMMON_LOGGING_H__

#include <fmt/format.h>
#include <stdarg.h>

namespace distbu {

std::string GetNowTime();

template<class... Ts>
std::string FormatLogOpt(const Ts&... args) noexcept {
    std::string result;
    if constexpr (sizeof...(Ts) != 0) {
        result += fmt::format(args...);
    }
    return result;
}

extern int min_log_level;

} // namespace distbu

#define DISTBU_FORMAT_LOG(...) distbu::FormatLogOpt(__VA_ARGS__).c_str()

#define DISTBU_LOG(level_str, level, ...)                           \
    if (level >= distbu::min_log_level) {                            \
        fprintf(stderr, "[%s]\t[%s]\t[%s:%d\t%s]\t%s\n",            \
                distbu::GetNowTime().c_str(), level_str,             \
                __FILE__, __LINE__, __func__,                       \
                DISTBU_FORMAT_LOG(__VA_ARGS__));                    \
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

#define DISTBU_ASSERT(expr, ...)                                    \
    do {                                                            \
        if (!(expr)) {                                              \
            LOG_FATAL("{}", fmt::format("Assert [{}] {}", #expr,    \
                      DISTBU_FORMAT_LOG(__VA_ARGS__)));             \
        }                                                           \
    } while(0)

#define DISTBU_CHECK(expr, ...)                                     \
    do {                                                            \
        if (!(expr)) {                                              \
            LOG_FATAL("Check failed: , {}"#expr,                    \
                    DISTBU_FORMAT_LOG(__VA_ARGS__));                \
        }                                                           \
    } while(0)

#endif
