#pragma once
/* =====================================================================
 *  event_log.h
 *  --------------------------------------------------------------
 *  Appends timestamped events to a CSV file on LittleFS. This is the
 *  file the Web UI serves for download, and "reset for next transit"
 *  just truncates it back to a fresh header.
 * =====================================================================
 */
#include <Arduino.h>

class EventLogger {
public:
    bool begin();     /* mounts LittleFS, creates file+header if missing */

    void logEvent(uint8_t eventType, float v1 = 0, float v2 = 0, float v3 = 0,
                  const char *note = "");

    /* Wipes the log and starts a fresh CSV -- used by "reset for next
     * transit" in the Web UI. */
    void resetLog();

    size_t fileSizeBytes();
    size_t entryCount();  /* approx, counts newlines minus header */

private:
    bool _ready = false;
};

extern EventLogger EventLog;
