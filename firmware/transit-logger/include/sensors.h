#pragma once
/* =====================================================================
 *  sensors.h
 *  --------------------------------------------------------------
 *  Wraps every I2C sensor on the board:
 *    - APDS9960  : ambient light / proximity / gesture
 *    - MPU6050   : 6-axis accel+gyro, used here mainly for shock/
 *                  motion detection during transit (drop, rough
 *                  handling, orientation change)
 *    - BMP180    : pressure / altitude, and our best temperature source
 *  Plus the battery-voltage ADC.
 *
 *  TEMPERATURE SOURCE PRIORITY (most to least accurate for AMBIENT air
 *  temperature, which is what matters for a package sitting in a box):
 *    1. BMP180  -- purpose-built ambient temp/pressure sensor, sits in
 *                  a plastic package with no significant self-heating,
 *                  quoted accuracy ~1-1.5 degC in typical datasheets.
 *    2. MPU6050 -- its internal temp sensor exists for gyro drift
 *                  compensation, not ambient sensing; it sits right
 *                  next to an active gyro die, so it reads a bit high.
 *                  Used only if BMP180 isn't present.
 *    3. ESP32 internal die sensor -- last resort only; the WiFi radio
 *                  self-heats this badly, especially while the AP is
 *                  running in Web UI mode, so treat it as a rough
 *                  proxy at best.
 * =====================================================================
 */
#include <Arduino.h>

struct SensorReadings {
    uint16_t ambientLux;
    uint8_t  proximity;         /* 0-255 */

    float    accelMagnitude_g;  /* vector magnitude of accel, in g;
                                    ~1.0g at rest, spikes on shock/drop */

    float    pressureHPa;
    float    altitudeM;         /* relative to sea-level-pressure constant
                                    below -- good for RELATIVE altitude
                                    change during transit, not absolute */

    float    batteryVoltage;
    uint8_t  batteryPercent;    /* 0-100 */

    float    temperatureC;      /* best available source, see priority above */
};

class SensorManager {
public:
    bool begin();

    /* Configures the APDS9960's own hardware proximity interrupt so it
     * can wake the ESP32 from deep sleep on a threshold crossing,
     * without the CPU having to poll while asleep. */
    void armProximityInterrupt(uint8_t proximityThreshold);
    void clearInterrupt();

    /* Configures + arms the MPU6050's motion-detection interrupt
     * (shock/drop/handling events during transit). */
    void armMotionInterrupt(uint8_t motionThreshold);
    void clearMotionInterrupt();

    SensorReadings readAll();

    /* Returns "" if no gesture, else "UP"/"DOWN"/"LEFT"/"RIGHT" */
    String pollGesture();

    bool apdsPresent() const { return _apdsPresent; }
    bool mpuPresent() const  { return _mpuPresent; }
    bool bmpPresent() const  { return _bmpPresent; }

private:
    bool _apdsPresent = false;
    bool _mpuPresent  = false;
    bool _bmpPresent  = false;

    float readBatteryVoltage();
    uint8_t batteryPercentFromVoltage(float v);
    float readBestTemperature();
};

extern SensorManager Sensors;
