#ifndef DISTRIBUILD_COMMON_LOGGING_H_
#define DISTRIBUILD_COMMON_LOGGING_H_

#include <fmt/format.h>
#include <stdarg.h>

namespace distribuild {

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

} // namespace distribuild

#define DISTRIBUILD_FORMAT_LOG(...) distribuild::FormatLogOpt(__VA_ARGS__).c_str()

#define DISTRIBUILD_LOG(level_str, level, ...)                      \
    if (level >= distribuild::min_log_level) {                      \
        fprintf(stderr, "[%s]\t[%s]\t[%s:%d\t%s]\t%s\n",            \
                distribuild::GetNowTime().c_str(), level_str,       \
                __FILE__, __LINE__, __func__,                       \
                DISTRIBUILD_FORMAT_LOG(__VA_ARGS__));               \
    }

#define LOG_DEBUG(...)  DISTRIBUILD_LOG("DEBUG", 0, __VA_ARGS__)
#define LOG_TRACE(...)  DISTRIBUILD_LOG("TRACE", 1, __VA_ARGS__)
#define LOG_INFO(...)   DISTRIBUILD_LOG("INFO",  2, __VA_ARGS__)
#define LOG_WARN(...)   DISTRIBUILD_LOG("WARN",  3, __VA_ARGS__)
#define LOG_ERROR(...)  DISTRIBUILD_LOG("ERROR", 4, __VA_ARGS__)
#define LOG_FATAL(...)                                              \
    do {                                                            \
        DISTRIBUILD_LOG("FATAL", 5, __VA_ARGS__);            \
        abort();                                                    \
    } while(0)

#define DISTRIBUILD_ASSERT(expr, ...)                               \
    do{                                                             \
        if (!(expr)) {                                              \
            LOG_FATAL("{}", fmt::format("Assert [{}] {}", #expr,    \
                      DISTRIBUILD_FORMAT_LOG(__VA_ARGS__)));        \
        }                                                           \
    } while(0)



#endif