#include "settings.h"
#include <Preferences.h>

SettingsManager Settings;
static Preferences prefs;

void SettingsManager::loadDefaults() {
    strncpy(_settings.apSsid, "TransitLogger-AP", sizeof(_settings.apSsid));
    strncpy(_settings.apPass, "12345678", sizeof(_settings.apPass));
    _settings.threshLux = 50.0f;
    _settings.threshTempC = 45.0f;
    _settings.motionThreshold = 20;
    _settings.loggingIntervalSec = 60;

    _settings.threshAccelX = 2.0f; // 2.0g default
    _settings.threshAccelY = 2.0f;
    _settings.threshAccelZ = 2.5f;
    _settings.threshPitch  = 45.0f; // 45 deg tilt default
    _settings.threshRoll   = 45.0f;

    _settings.calibOffsetX = 0.0f;
    _settings.calibOffsetY = 0.0f;
    _settings.calibOffsetZ = 0.0f;
}

bool SettingsManager::begin() {
    prefs.begin("settings", false);
    loadDefaults();

    _settings.threshLux           = prefs.getFloat("lux", _settings.threshLux);
    _settings.threshTempC         = prefs.getFloat("temp", _settings.threshTempC);
    _settings.motionThreshold     = prefs.getUChar("motion", _settings.motionThreshold);
    _settings.loggingIntervalSec  = prefs.getULong("interval", _settings.loggingIntervalSec);

    _settings.threshAccelX        = prefs.getFloat("th_ax", _settings.threshAccelX);
    _settings.threshAccelY        = prefs.getFloat("th_ay", _settings.threshAccelY);
    _settings.threshAccelZ        = prefs.getFloat("th_az", _settings.threshAccelZ);
    _settings.threshPitch         = prefs.getFloat("th_pitch", _settings.threshPitch);
    _settings.threshRoll          = prefs.getFloat("th_roll", _settings.threshRoll);

    _settings.calibOffsetX        = prefs.getFloat("cal_x", 0.0f);
    _settings.calibOffsetY        = prefs.getFloat("cal_y", 0.0f);
    _settings.calibOffsetZ        = prefs.getFloat("cal_z", 0.0f);
    return true;
}

void SettingsManager::setBasicThresholds(float lux, float temp) {
    _settings.threshLux = lux;
    _settings.threshTempC = temp;
    save();
}

void SettingsManager::setDirectionalThresholds(float ax, float ay, float az, float pitch, float roll) {
    _settings.threshAccelX = ax;
    _settings.threshAccelY = ay;
    _settings.threshAccelZ = az;
    _settings.threshPitch  = pitch;
    _settings.threshRoll   = roll;
    save();
}

void SettingsManager::setMotionThreshold(uint8_t m) {
    _settings.motionThreshold = m;
    save();
}

void SettingsManager::setLoggingInterval(uint32_t sec) {
    _settings.loggingIntervalSec = sec;
    save();
}


void SettingsManager::setCalibrationOffsets(float ox, float oy, float oz) {
    _settings.calibOffsetX = ox;
    _settings.calibOffsetY = oy;
    _settings.calibOffsetZ = oz;
    save();
}

void SettingsManager::save() {
    prefs.putFloat("lux", _settings.threshLux);
    prefs.putFloat("temp", _settings.threshTempC);
    prefs.putUChar("motion", _settings.motionThreshold);
    prefs.putULong("interval", _settings.loggingIntervalSec);

    prefs.putFloat("th_ax", _settings.threshAccelX);
    prefs.putFloat("th_ay", _settings.threshAccelY);
    prefs.putFloat("th_az", _settings.threshAccelZ);
    prefs.putFloat("th_pitch", _settings.threshPitch);
    prefs.putFloat("th_roll", _settings.threshRoll);

    prefs.putFloat("cal_x", _settings.calibOffsetX);
    prefs.putFloat("cal_y", _settings.calibOffsetY);
    prefs.putFloat("cal_z", _settings.calibOffsetZ);
}