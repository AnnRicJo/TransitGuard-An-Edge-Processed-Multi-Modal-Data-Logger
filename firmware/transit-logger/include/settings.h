#pragma once
/* =====================================================================
 *  settings.h
 *  --------------------------------------------------------------
 *  Wraps the ESP32 Preferences (NVS) library to persist:
 *    - alert thresholds (lux / proximity / temperature)
 *    - the "home" Wi-Fi credentials used only briefly to fetch NTP time
 *    - the device's own AP SSID/password
 *  All of this is editable from the Web UI dashboard.
 * =====================================================================
 */
#include <Arduino.h>

struct DeviceSettings {
    float   threshLux;
    float   threshProximity;
    float   threshTempC;
    uint8_t motionThreshold;   /* MPU6050 shock/motion sensitivity, 1-255 */

    uint32_t loggingIntervalSec; /* time between successive logged sensor-
                                     data rows, set by the user in the Web
                                     UI; data is also logged immediately on
                                     any threshold-breach interrupt
                                     (motion/proximity/lux/temp) regardless
                                     of this interval */

    char    homeWifiSsid[33];
    char    homeWifiPass[65];

    char    apSsid[33];
    char    apPass[65];
};

class SettingsManager {
public:
    void begin();                       /* loads from NVS, or writes defaults */
    const DeviceSettings &get() const { return _settings; }

    void setThresholds(float lux, float prox, float tempC);
    void setMotionThreshold(uint8_t thr);
    void setLoggingInterval(uint32_t sec);
    void setHomeWifi(const char *ssid, const char *pass);
    void setApCredentials(const char *ssid, const char *pass);

    void resetToDefaults();

private:
    DeviceSettings _settings;
    void load();
    void save();
};

extern SettingsManager Settings;
