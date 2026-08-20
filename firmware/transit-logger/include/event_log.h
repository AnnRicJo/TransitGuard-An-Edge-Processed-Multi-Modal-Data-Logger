#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensors.h"

class EventLogManager {
public:
    bool begin();
    void logEvent(EventType evt, const SensorReadings &r, const char *note = "");
    void resetLog();
    size_t entryCount();
    const char* eventTypeToString(EventType evt);

private:
    void initHeader();
};

extern EventLogManager EventLog;