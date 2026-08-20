#pragma once
#include <Arduino.h>

struct DeviceSettings {
    char     apSsid[32];
    char     apPass[32];

    float    threshLux;
    float    threshTempC;
    uint8_t  motionThreshold;
    uint32_t loggingIntervalSec;

    // Directional shock thresholds (in units of g)
    float    threshAccelX;
    float    threshAccelY;
    float    threshAccelZ;

    // Tilt limits (in degrees)
    float    threshPitch;
    float    threshRoll;

    // Calibration biases
    float    calibOffsetX;
    float    calibOffsetY;
    float    calibOffsetZ;
};

class SettingsManager {
public:
    bool begin();
    const DeviceSettings& get() const { return _settings; }
    
    void setBasicThresholds(float lux, float temp);
    void setDirectionalThresholds(float ax, float ay, float az, float pitch, float roll);
    void setMotionThreshold(uint8_t m);
    void setLoggingInterval(uint32_t sec);
    void setCalibrationOffsets(float ox, float oy, float oz);
    void save();

private:
    DeviceSettings _settings;
    void loadDefaults();
};

extern SettingsManager Settings;