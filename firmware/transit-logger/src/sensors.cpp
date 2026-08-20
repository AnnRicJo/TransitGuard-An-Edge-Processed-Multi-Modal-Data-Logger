#include "sensors.h"
#include "config.h"
#include "settings.h"
#include <Wire.h>
#include <Adafruit_APDS9960.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <math.h>

SensorManager Sensors;

static Adafruit_APDS9960 apds;
static Adafruit_MPU6050  mpu;
static Adafruit_BMP085   bmp;

#define SEA_LEVEL_HPA 1013.25f

bool SensorManager::begin() {
    analogSetPinAttenuation(BATT_ADC_PIN, ADC_11db);
    pinMode(BATT_ADC_PIN, INPUT);
    delay(50);

    _apdsPresent = apds.begin();
    if (_apdsPresent) {
        apds.enableGesture(false);
        apds.enableColor(true);
        apds.setADCIntegrationTime(219);
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

    return _apdsPresent || _mpuPresent || _bmpPresent;
}

void SensorManager::clearInterrupt() {
    if (_apdsPresent) {
        apds.clearInterrupt();
    }
}

void SensorManager::armMotionInterrupt(uint8_t motionThreshold) {
    if (!_mpuPresent) return;

    // 1. Clear any latched interrupt status before reconfiguring
    clearMotionInterrupt();

    // 2. Configure High-Pass Filter and Motion Detection Engine
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    
    // Ensure threshold is not in the noise floor (minimum 25-30 LSB ~ 0.8g-1.0g delta)
    uint8_t safeThreshold = (motionThreshold < 20) ? 25 : motionThreshold;
    mpu.setMotionDetectionThreshold(safeThreshold);
    mpu.setMotionDetectionDuration(MPU_MOTION_DURATION_SAMPLES); // at least 4-5 samples

    // 3. Configure INT pin as Active-HIGH, latched until status read
    mpu.setInterruptPinLatch(true);
    mpu.setInterruptPinPolarity(true); // Active-HIGH
    mpu.setMotionInterrupt(true);

    // 4. CRITICAL: Wait 30ms for the high-pass filter to settle, then clear spurious initial flag
    delay(30);
    clearMotionInterrupt();
}

void SensorManager::clearMotionInterrupt() {
    if (!_mpuPresent) return;
    
    // Read register 0x3A (INT_STATUS) over I2C to clear hardware latch
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(0x3A);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU6050_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        Wire.read();
    }
}

void SensorManager::calibrateZero() {
    if (!_mpuPresent) return;
    float sumX = 0, sumY = 0, sumZ = 0;
    const int SAMPLES = 30;
    sensors_event_t a, g, tempEvent;

    for (int i = 0; i < SAMPLES; i++) {
        mpu.getEvent(&a, &g, &tempEvent);
        sumX += a.acceleration.x / 9.80665f;
        sumY += a.acceleration.y / 9.80665f;
        sumZ += (a.acceleration.z / 9.80665f) - 1.0f; // Target is +1.0g on Z when upright
        delay(20);
    }
    Settings.setCalibrationOffsets(sumX / SAMPLES, sumY / SAMPLES, sumZ / SAMPLES);
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

    if (_apdsPresent) {
        uint32_t start = millis();
        while (!apds.colorDataReady() && (millis() - start < 120)) delay(5);
        uint16_t red = 0, green = 0, blue = 0, clearCh = 0;
        apds.getColorData(&red, &green, &blue, &clearCh);
        r.ambientLux = (clearCh > 0) ? apds.calculateLux(red, green, blue) : 0.0f;
        if (r.ambientLux == 0.0f && clearCh > 0) r.ambientLux = (float)clearCh;
    } else {
        r.ambientLux = 0.0f;
    }

    if (_mpuPresent) {
        sensors_event_t a, g, tempEvent;
        mpu.getEvent(&a, &g, &tempEvent);

        const DeviceSettings &s = Settings.get();
        // Apply calibration offsets
        r.accelX = (a.acceleration.x / 9.80665f) - s.calibOffsetX;
        r.accelY = (a.acceleration.y / 9.80665f) - s.calibOffsetY;
        r.accelZ = (a.acceleration.z / 9.80665f) - s.calibOffsetZ;
        r.accelMagnitude_g = sqrtf(r.accelX * r.accelX + r.accelY * r.accelY + r.accelZ * r.accelZ);

        r.pitchDeg = atan2f(r.accelX, sqrtf(r.accelY * r.accelY + r.accelZ * r.accelZ)) * (180.0f / M_PI);
        r.rollDeg  = atan2f(r.accelY, sqrtf(r.accelX * r.accelX + r.accelZ * r.accelZ)) * (180.0f / M_PI);

        if (r.accelZ > 0.7f)       r.orientation = "TOP_UP";
        else if (r.accelZ < -0.7f) r.orientation = "UPSIDE_DOWN";
        else if (r.accelX > 0.7f)  r.orientation = "TILT_X_POS";
        else if (r.accelX < -0.7f) r.orientation = "TILT_X_NEG";
        else if (r.accelY > 0.7f)  r.orientation = "TILT_Y_POS";
        else if (r.accelY < -0.7f) r.orientation = "TILT_Y_NEG";
        else                       r.orientation = "ANGLED";
    } else {
        r.orientation = "UNKNOWN";
    }

    if (_bmpPresent) {
        r.pressureHPa = bmp.readPressure() / 100.0f;
        r.altitudeM   = bmp.readAltitude(SEA_LEVEL_HPA * 100.0f);
    } else {
        r.pressureHPa = 0;
        r.altitudeM   = 0;
    }
    return r;
}