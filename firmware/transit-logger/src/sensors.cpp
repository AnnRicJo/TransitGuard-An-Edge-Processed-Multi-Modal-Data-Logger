#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_APDS9960.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>

SensorManager Sensors;

static Adafruit_APDS9960 apds;
static Adafruit_MPU6050  mpu;
static Adafruit_BMP085   bmp;

/* Standard sea-level pressure constant used for relative altitude calculation */
#define SEA_LEVEL_HPA 1013.25f

bool SensorManager::begin() {
    analogSetPinAttenuation(BATT_ADC_PIN, ADC_11db);
    pinMode(BATT_ADC_PIN, INPUT);
    delay(50);

    _apdsPresent = apds.begin();
    if (_apdsPresent) {
        // Disable proximity and gestures, enable ONLY color/ambient light
        apds.enableProximity(false);
        apds.enableGesture(false);
        apds.enableColor(true);

        // Configure Ambient Light ADC integration time & gain
        apds.setADCIntegrationTime(219); // ~103ms integration time
        apds.setADCGain(APDS9960_AGAIN_4X);
    } else {
        Serial.println(F("[Sensors] APDS9960 not responding"));
    }

    _mpuPresent = mpu.begin(MPU6050_I2C_ADDR);
    if (_mpuPresent) {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        pinMode(MPU_INT_PIN, INPUT);
    } else {
        Serial.println(F("[Sensors] MPU6050 not responding"));
    }

    _bmpPresent = bmp.begin();
    if (!_bmpPresent) {
        Serial.println(F("[Sensors] BMP180 not responding"));
    }

    analogSetPinAttenuation(BATT_ADC_PIN, ADC_11db);

    return _apdsPresent || _mpuPresent || _bmpPresent;
}

void SensorManager::armProximityInterrupt(uint8_t proximityThreshold) {
    (void)proximityThreshold; // No-op: Proximity interrupt removed
}

void SensorManager::clearInterrupt() {
    if (!_apdsPresent) return;
    apds.clearInterrupt();
}

void SensorManager::armMotionInterrupt(uint8_t motionThreshold) {
    if (!_mpuPresent) return;
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    mpu.setMotionDetectionThreshold(motionThreshold);
    mpu.setMotionDetectionDuration(MPU_MOTION_DURATION_SAMPLES);
    mpu.setInterruptPinLatch(true);
    mpu.setInterruptPinPolarity(false);
    mpu.setMotionInterrupt(true);
}

void SensorManager::clearMotionInterrupt() {
    if (!_mpuPresent) return;
    mpu.getMotionInterruptStatus();
}

float SensorManager::readBatteryVoltage() {
    uint32_t sum = 0;
    const int N = 8;
    for (int i = 0; i < N; i++) {
        sum += analogRead(BATT_ADC_PIN);
        delayMicroseconds(200);
    }
    float raw = sum / (float)N;
    float pinVoltage = (raw / 4095.0f) * BATT_ADC_VREF;
    return pinVoltage * BATT_DIVIDER_RATIO;
}

uint8_t SensorManager::batteryPercentFromVoltage(float v) {
    if (v >= BATT_FULL_VOLTAGE) return 100;
    if (v <= BATT_EMPTY_VOLTAGE) return 0;
    float pct = (v - BATT_EMPTY_VOLTAGE) / (BATT_FULL_VOLTAGE - BATT_EMPTY_VOLTAGE) * 100.0f;
    return (uint8_t)constrain(pct, 0.0f, 100.0f);
}

float SensorManager::readBestTemperature() {
    if (_bmpPresent) {
        return bmp.readTemperature();
    }
    if (_mpuPresent) {
        sensors_event_t a, g, tempEvent;
        mpu.getEvent(&a, &g, &tempEvent);
        return tempEvent.temperature;
    }
    return temperatureRead();
}

SensorReadings SensorManager::readAll() {
    SensorReadings r{};
    r.batteryVoltage = readBatteryVoltage();
    r.batteryPercent = batteryPercentFromVoltage(r.batteryVoltage);
    r.temperatureC   = readBestTemperature();
    r.proximity      = 0; // Proximity disabled

    if (_apdsPresent) {
        uint32_t start = millis();
        while (!apds.colorDataReady() && (millis() - start < 120)) {
            delay(5);
        }

        uint16_t red = 0, green = 0, blue = 0, clearCh = 0;
        apds.getColorData(&red, &green, &blue, &clearCh);

        if (clearCh > 0) {
            r.ambientLux = apds.calculateLux(red, green, blue);
            if (r.ambientLux == 0.0f) {
                r.ambientLux = (float)clearCh;
            }
        } else {
            r.ambientLux = 0.0f;
        }
    } else {
        r.ambientLux = 0.0f;
    }

    if (_mpuPresent) {
        sensors_event_t a, g, tempEvent;
        mpu.getEvent(&a, &g, &tempEvent);
        float ax = a.acceleration.x, ay = a.acceleration.y, az = a.acceleration.z;
        float magMs2 = sqrtf(ax * ax + ay * ay + az * az);
        r.accelMagnitude_g = magMs2 / 9.80665f;
    } else {
        r.accelMagnitude_g = 0;
    }

    if (_bmpPresent) {
        r.pressureHPa = bmp.readPressure() / 100.0f;
        r.altitudeM   = bmp.readAltitude(SEA_LEVEL_HPA * 100.0f);
    } else {
        r.pressureHPa = 0;
        r.altitudeM = 0;
    }

    return r;
}

String SensorManager::pollGesture() {
    return ""; // Gesture engine disabled
}