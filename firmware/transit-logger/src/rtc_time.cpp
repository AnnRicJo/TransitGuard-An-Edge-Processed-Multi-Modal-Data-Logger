#include "rtc_time.h"
#include <sys/time.h>

RtcTimeManager RtcTime;

RTC_DATA_ATTR static bool s_timeSynced = false;
RTC_DATA_ATTR static int32_t s_tzOffsetSec = 0;

void RtcTimeManager::begin() {
}

void RtcTimeManager::setEpochTime(time_t epochSec, int32_t tzOffsetSec) {
    s_tzOffsetSec = tzOffsetSec;
    struct timeval tv;
    tv.tv_sec = epochSec + tzOffsetSec; // Adjust to local wall-clock time
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    s_timeSynced = true;
}

bool RtcTimeManager::isSynced() const {
    return s_timeSynced;
}

time_t RtcTimeManager::nowEpoch() const {
    return time(NULL);
}

String RtcTimeManager::nowFormatted() const {
    time_t now = time(NULL);
    if (!s_timeSynced) {
        char buf[32];
        uint32_t sec = (uint32_t)now;
        uint32_t days = sec / 86400;
        uint32_t rem  = sec % 86400;
        uint32_t hrs  = rem / 3600;
        uint32_t mins = (rem % 3600) / 60;
        uint32_t s    = rem % 60;
        snprintf(buf, sizeof(buf), "T+%03lud %02lu:%02lu:%02lu",
                 (unsigned long)days, (unsigned long)hrs,
                 (unsigned long)mins, (unsigned long)s);
        return String(buf);
    }

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo); // Use gmtime_r since the offset was added to the base clock
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}