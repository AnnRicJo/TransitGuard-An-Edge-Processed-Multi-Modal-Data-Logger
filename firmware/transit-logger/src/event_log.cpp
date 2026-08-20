#include "event_log.h"
#include "config.h"
#include "rtc_time.h"
#include <LittleFS.h>

EventLogManager EventLog;

bool EventLogManager::begin() {
    if (!LittleFS.begin(true)) {
        return false;
    }
    initHeader();
    return true;
}

void EventLogManager::initHeader() {
    if (!LittleFS.exists(LOG_FILE_PATH)) {
        File f = LittleFS.open(LOG_FILE_PATH, "w");
        if (f) {
            f.println("timestamp,event_type,lux,temp_c,press_hpa,accel_x,accel_y,accel_z,mag_g,pitch,roll,orientation,batt_pct,note");
            f.close();
        }
    }
}

const char* EventLogManager::eventTypeToString(EventType evt) {
    switch (evt) {
        case EVT_BOOT:            return "BOOT";
        case EVT_PERIODIC:        return "PERIODIC";
        case EVT_MOTION:          return "MOTION";
        case EVT_TAMPER:          return "TAMPER";
        case EVT_LIGHT_THRESHOLD: return "LIGHT_ALERT";
        case EVT_TEMP_THRESHOLD:  return "TEMP_ALERT";
        case EVT_LOW_BATTERY:     return "LOW_BATTERY";
        case EVT_MODE_CHANGE:     return "MODE_CHANGE";
        case EVT_BUTTON_DISPLAY:  return "BUTTON_DISPLAY";
        case EVT_LOG_RESET:       return "LOG_RESET";
        default:                  return "UNKNOWN";
    }
}

void EventLogManager::logEvent(EventType evt, const SensorReadings &r, const char *note) {
    File f = LittleFS.open(LOG_FILE_PATH, "a");
    if (!f) return;

    f.printf("%s,%s,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%s,%u,%s\n",
             RtcTime.nowFormatted().c_str(),
             eventTypeToString(evt),
             r.ambientLux,
             r.temperatureC,
             r.pressureHPa,
             r.accelX,
             r.accelY,
             r.accelZ,
             r.accelMagnitude_g,
             r.pitchDeg,
             r.rollDeg,
             r.orientation,
             r.batteryPercent,
             note ? note : "");
    f.close();
}

void EventLogManager::resetLog() {
    LittleFS.remove(LOG_FILE_PATH);
    initHeader();
    SensorReadings r = Sensors.readAll();
    logEvent(EVT_LOG_RESET, r, "new-transit-started");
}

size_t EventLogManager::entryCount() {
    if (!LittleFS.exists(LOG_FILE_PATH)) return 0;
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    if (!f) return 0;
    size_t lines = 0;
    while (f.available()) {
        if (f.read() == '\n') lines++;
    }
    f.close();
    return (lines > 1) ? (lines - 1) : 0;
}