#pragma once
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

class RtcTimeManager {
public:
    void begin();
    void setEpochTime(time_t epochSec, int32_t tzOffsetSec = 0);
    bool isSynced() const;
    time_t nowEpoch() const;
    String nowFormatted() const;
};

extern RtcTimeManager RtcTime;