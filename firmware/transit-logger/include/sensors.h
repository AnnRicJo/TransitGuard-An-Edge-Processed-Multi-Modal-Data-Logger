#pragma once
#include <Arduino.h>

struct SensorReadings {
    float batteryVoltage;
    uint8_t batteryPercent;
    float temperatureC;
    float pressureHPa;
    float altitudeM;
    float ambientLux;
    float accelX;
    float accelY;
    float accelZ;
    float accelMagnitude_g;
    float pitchDeg;
    float rollDeg;
    const char* orientation;
};

class SensorManager {
public:
    bool begin();
    SensorReadings readAll();
    void armMotionInterrupt(uint8_t motionThreshold);
    void clearMotionInterrupt();
    void calibrateZero();
    void clearInterrupt();
    bool apdsPresent() const { return _apdsPresent; }
    bool mpuPresent()  const { return _mpuPresent; }
    bool bmpPresent()  const { return _bmpPresent; }
    void configureMotionInterrupt(uint8_t motionThreshold);

private:
    bool _apdsPresent = false;
    bool _mpuPresent  = false;
    bool _bmpPresent  = false;
    float readBatteryVoltage();
    uint8_t batteryPercentFromVoltage(float v);
    float readBestTemperature();
};

extern SensorManager Sensors;