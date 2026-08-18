#pragma once
/* =====================================================================
 *  display.h
 *  --------------------------------------------------------------
 *  Drives the SSD1306 OLED. Distinct "screens":
 *    - drawBootSplash()         : shown once, right at power-on, while
 *                                 peripherals are still being initialised
 *    - drawSensorStatusScreen() : continuous post-boot screen -- per-
 *                                 sensor recognised/active status
 *                                 (MPU6050/BMP180/APDS9960) + battery.
 *                                 Shown until the UI (mode) button is
 *                                 pressed, which starts Web UI mode
 *    - drawWebUiInfoScreen()    : persistent Web UI mode screen -- AP
 *                                 URL, "web UI mode" indicator, battery,
 *                                 time
 *    - drawLowPowerInfoScreen() : low-power-mode "display" button screen
 *                                 (5s only) -- battery, time, and the
 *                                 package's safety status (thresholds +
 *                                 tamper)
 *  All screens show a tamper banner when TamperSensor is latched.
 * =====================================================================
 */
#include <Arduino.h>
#include "sensors.h"

class DisplayManager {
public:
    bool begin();

    void sleep();   /* SSD1306 low-power display-off (controller stays on I2C) */
    void wake();

    /* Continuous post-boot sensor-recognition + battery screen. */
    void drawSensorStatusScreen(bool apdsPresent, bool mpuPresent, bool bmpPresent,
                                 const SensorReadings &r);

    /* Persistent Web UI mode screen: URL to open, mode indicator,
     * battery, time. */
    void drawWebUiInfoScreen(const char *url, const SensorReadings &r,
                              uint8_t clientsConnected, bool tampered);

    /* Low-power-mode "display" button screen: battery, time, safety
     * status of the package contents (threshold breaches + tamper). */
    void drawLowPowerInfoScreen(const SensorReadings &r, bool tampered,
                                 bool luxBreached, bool tempBreached);

    void drawBootSplash();
    void drawMessage(const char *line1, const char *line2 = "");

private:
    bool _present = false;
    void drawTamperBanner();
    void drawWifiGlyph(int x, int y, bool active);
};

extern DisplayManager Display;
