#include "logging.h"

#include <sys/time.h>
#include <time.h>

namespace distribuild {

std::string GetNowTime() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t time_now = tv.tv_sec;
    struct tm time_info;
    localtime_r(&time_now, &time_info);
    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:02d}",
                        time_info.tm_year + 1900, time_info.tm_mon + 1,time_info.tm_mday, 
                        time_info.tm_hour, time_info.tm_min, time_info.tm_sec, tv.tv_usec);
}

} // namespace distribuild