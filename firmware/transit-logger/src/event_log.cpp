#include "event_log.h"
#include "config.h"
#include "rtc_time.h"
#include <LittleFS.h>

EventLogger EventLog;

bool EventLogger::begin() {
    if (!LittleFS.begin(true /* formatOnFail */)) {
        Serial.println(F("[EventLog] LittleFS mount FAILED"));
        _ready = false;
        return false;
    }

    if (!LittleFS.exists(LOG_FILE_PATH)) {
        File f = LittleFS.open(LOG_FILE_PATH, "w");
        if (f) {
            f.print(LOG_CSV_HEADER);
            f.close();
        }
    }
    _ready = true;
    return true;
}

void EventLogger::logEvent(uint8_t eventType, float v1, float v2, float v3, const char *note) {
    if (!_ready) return;

    File f = LittleFS.open(LOG_FILE_PATH, "a");
    if (!f) {
        Serial.println(F("[EventLog] append open FAILED"));
        return;
    }

    f.print(RtcTime.nowFormatted());
    f.print(',');
    f.print(eventTypeToStr(eventType));
    f.print(',');
    f.print(v1, 2);
    f.print(',');
    f.print(v2, 2);
    f.print(',');
    f.print(v3, 2);
    f.print(',');
    f.println(note);

    f.close();
}

void EventLogger::resetLog() {
    if (!_ready) return;
    LittleFS.remove(LOG_FILE_PATH);
    File f = LittleFS.open(LOG_FILE_PATH, "w");
    if (f) {
        f.print(LOG_CSV_HEADER);
        f.close();
    }
    logEvent(EVT_LOG_RESET, 0, 0, 0, "new-transit-started");
}

size_t EventLogger::fileSizeBytes() {
    if (!_ready || !LittleFS.exists(LOG_FILE_PATH)) return 0;
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    size_t sz = f.size();
    f.close();
    return sz;
}

size_t EventLogger::entryCount() {
    if (!_ready || !LittleFS.exists(LOG_FILE_PATH)) return 0;
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    size_t lines = 0;
    while (f.available()) {
        if (f.read() == '\n') lines++;
    }
    f.close();
    return (lines > 0) ? (lines - 1) : 0; /* minus header line */
}
