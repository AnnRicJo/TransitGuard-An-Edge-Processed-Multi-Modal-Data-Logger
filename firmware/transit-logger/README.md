# Transit Package Logger — Firmware

Firmware for the ESP32-based MYOSA Mini Kit used in the Transit Package Logger.

The firmware has two main operating modes:

- **Low-power logging:** the ESP32 spends most of its time in deep sleep and wakes to record events or periodic sensor readings.
- **Web UI:** the ESP32 creates its own Wi-Fi access point and hosts a browser dashboard for viewing sensor data, changing thresholds, downloading the CSV log, and preparing the device for the next transit.

The firmware is written with **PlatformIO + Arduino framework**.

---

## Hardware

| Component | Interface / Address | Purpose |
|---|---|---|
| MYOSA Mini Kit / ESP32-WROOM-32E | — | Main controller |
| SSD1306 OLED | I2C `0x3C` | Status and package information |
| APDS9960 | I2C `0x39` | Ambient-light measurement |
| MPU6050 | I2C `0x69` | Motion/shock detection |
| BMP180 | I2C `0x77` | Temperature and pressure |
| Passive piezo buzzer | GPIO | Audible feedback and alerts |
| Aluminium-foil tamper sensor | ESP32 Touch0 | Tamper detection |
| LiPo battery | ADC through divider | Battery-voltage measurement |
| MYOSA onboard LED | GPIO2 | Firmware status indication |

### Current sensor configuration

The final firmware uses the APDS9960 **only for ambient-light sensing**.

Proximity and gesture detection are disabled in the current implementation. The corresponding proximity/gesture functions remain in the code structure, but they are not active features.

Temperature is selected automatically:

1. BMP180 temperature, when the BMP180 is detected.
2. MPU6050 internal temperature, if the BMP180 is unavailable.
3. ESP32 internal temperature as the final fallback.

---

## Pinout

| Signal | GPIO | Notes |
|---|---:|---|
| I2C SDA | `21` | Shared by OLED, APDS9960, MPU6050 and BMP180 |
| I2C SCL | `22` | Shared I2C bus |
| Mode / UI button | `32` | Active-HIGH, internal pulldown |
| Display button | `33` | Active-HIGH, internal pulldown |
| APDS9960 INT | `25` | Reserved in the current hardware configuration; proximity interrupt is disabled |
| MPU6050 INT | `27` | Motion/shock interrupt |
| Tamper foil | `4` | ESP32 Touch0 |
| Battery ADC | `34` | ADC input through a 2:1 resistor divider |
| Buzzer | `19` | Passive piezo driven directly by GPIO |
| Onboard LED | `2` | MYOSA status LED |

### Button wiring

Both buttons are **active-HIGH**:

```text
3.3V ──[ BUTTON ]── GPIO
                     │
                 internal
                 pulldown
```

- Idle = `LOW`
- Pressed = `HIGH`

This polarity is required by the current deep-sleep `EXT1` wake configuration, which uses `ANY_HIGH` for the buttons and MPU6050 interrupt.

---

## Firmware behaviour

### 1. Power-on

After a true power-on:

1. The OLED displays the Transit Logger boot screen.
2. The firmware initialises the sensors, display, filesystem, battery measurement and tamper system.
3. A short buzzer confirmation is given.
4. The OLED continuously displays sensor detection status and battery level.
5. Pressing the **Mode / UI button** enters Web UI mode.

The sensor-status screen does **not** automatically time out.

---

### 2. Low-power logging mode

In logging mode, the OLED is normally off and the ESP32 enters deep sleep.

The device wakes from:

- MPU6050 motion/shock interrupt
- Mode button
- Display button
- Capacitive tamper wake
- Configured periodic logging timer

After waking, the firmware:

1. Reads the available sensors.
2. Determines the wake/event reason.
3. Records the appropriate event in the CSV log.
4. Checks tamper and light/temperature thresholds.
5. Records a low-battery event if the battery is at or below 10%.
6. Returns to deep sleep unless a button requests another mode.

### Periodic logging

The default logging interval is:

```text
600 seconds (10 minutes)
```

The interval can be changed from the Web UI and is limited to:

```text
30 seconds – 86400 seconds
```

A periodic wake records a sensor snapshot even when no other event has occurred.

---

## Motion / shock detection

The MPU6050 is configured for motion detection using its hardware interrupt.

Current configuration:

- Accelerometer range: **±8 g**
- Gyroscope range: **±500 °/s**
- Filter bandwidth: **21 Hz**
- Motion threshold: configurable, default `8`
- Motion duration: `20` samples
- Interrupt: GPIO `27`

The motion sensitivity can be changed from the Web UI.

The firmware also records the current acceleration magnitude in `g`.

> The motion threshold should be tested with the final enclosure and packing material because normal transport vibration can generate motion events.

---

## Tamper detection

The aluminium-foil lining is connected to the ESP32's capacitive-touch input on **GPIO4 / Touch0**.

On the first setup, the firmware takes an averaged baseline from 10 touch readings.

A significant deviation from that baseline is treated as tampering.

The tamper state is **latched in ESP32 NVS**, meaning it remains recorded until explicitly cleared from the Web UI.

Tamper can therefore remain recorded even after a reset or battery removal.

### Initial calibration

The first calibration should be performed with the box in its normal sealed configuration and without touching the foil.

The allowed deviation is currently defined in `src/tamper.cpp`:

```cpp
#define TAMPER_ALLOWED_DELTA 20
```

This value may need to be tuned for the final enclosure and foil geometry.

---

## Battery monitoring

The charger module does not provide digital battery telemetry.

Battery voltage is therefore measured by the ESP32 through:

- ADC pin: `GPIO34`
- Resistor divider: `2:1`
- ADC reference used by firmware: `3.3 V`

The firmware maps:

```text
4.2 V → 100%
3.3 V → 0%
```

The battery percentage shown by the firmware is therefore a voltage-based estimate rather than a fuel-gauge measurement.

If the actual resistor divider differs from the current design, update:

```cpp
BATT_DIVIDER_RATIO
```

in `include/config.h`.

---

## OLED displays

The SSD1306 OLED uses I2C address `0x3C`.

The firmware has four main display states:

### Boot screen

Shown briefly after power-on.

### Sensor status

Shown continuously after a true power-on until the Mode button is pressed.

It shows:

- MPU6050 status
- BMP180 status
- APDS9960 status
- Battery voltage and percentage

### Web UI mode

Shows:

- Web UI mode indicator
- `transitlogger.local`
- Battery voltage and percentage
- Device time

### Low-power information screen

Pressing the Display button during logging mode wakes the OLED for **5 seconds**.

It shows:

- Battery status
- Device time
- Package safety status
- Tamper / light / temperature alerts when applicable

The Display button has no function while Web UI mode is active.

---

## Buzzer and status LED

### Buzzer

The passive piezo buzzer is connected directly to GPIO19.

A short beep confirms button/actions.

Alert beeps are produced when an applicable threshold or tamper condition is active **while the OLED is being shown**.

Background deep-sleep logging does not continuously sound the buzzer.

### Onboard LED

GPIO2 is used as the MYOSA status LED:

| State | LED |
|---|---|
| Deep-sleep logging | OFF |
| Wake/event activity | Brief blink |
| Web UI mode | Solid ON |
| Tamper latched in Web UI | Fast blink |

---

# Web UI

Press the **Mode / UI button** to enter Web UI mode.

The ESP32 creates a Wi-Fi access point using the configured SSID and password.

The default SSID prefix is:

```text
TransitLogger-
```

The last three bytes of the ESP32 MAC address are appended to make the default SSID unique.

The default AP password is defined in:

```text
include/config.h
```

**Change the default password before deployment.**

## Opening the dashboard

After connecting to the ESP32's Wi-Fi network, open:

```text
http://transitlogger.local
```

The firmware also runs a captive-portal-style DNS server, so opening another address in the browser can redirect to the dashboard.

If automatic captive-portal detection does not appear, use the `transitlogger.local` address.

---

## Dashboard functions

The Web UI provides:

### Live sensor data

- Ambient light
- Battery voltage and percentage
- Temperature
- Pressure
- Acceleration magnitude
- Device time
- Number of logged events
- Tamper status

The live status is refreshed approximately every **3 seconds**.

### Threshold configuration

The following values can be changed:

- Ambient-light threshold
- Temperature threshold
- MPU6050 motion sensitivity
- Logging interval

These settings are stored in ESP32 NVS and survive resets.

### Time synchronisation

The dashboard includes **Sync time with this device**.

This sends the browser's current Unix time and timezone offset to the ESP32.

The firmware does **not** currently perform an automatic NTP connection. Time synchronisation is performed through the Web UI.

### CSV download

The current event log can be downloaded from the dashboard as:

```text
transit_log.csv
```

### Reset for next transit

The reset function clears the existing event log and creates a new log beginning with a `LOG_RESET` event.

### Return to low-power mode

The Web UI provides a **Go to sleep now** button.

Pressing the physical Mode button has the same effect.

Saving the logging settings also returns the device to low-power logging mode.

---

# Event logging

Events are stored in:

```text
/events.csv
```

on the ESP32's LittleFS filesystem.

The CSV format is:

```csv
timestamp,event_type,value1,value2,value3,note
```

Possible event types currently defined by the firmware include:

- `BOOT`
- `PERIODIC`
- `PROXIMITY`
- `LIGHT_THRESHOLD`
- `GESTURE`
- `TEMP_THRESHOLD`
- `TAMPER`
- `MODE_CHANGE`
- `BUTTON_DISPLAY`
- `LOG_RESET`
- `LOW_BATTERY`
- `MOTION`

`PROXIMITY` and `GESTURE` event types remain defined for compatibility with the event system, but the corresponding APDS9960 features are disabled in the current firmware.

Light and temperature threshold events are logged on the transition into a breached state rather than repeatedly logging the same continuous breach.

---

# Project structure

```text
transit-logger/
├── include/
│   ├── buzzer.h
│   ├── config.h
│   ├── display.h
│   ├── event_log.h
│   ├── rtc_time.h
│   ├── sensors.h
│   ├── settings.h
│   ├── tamper.h
│   └── webui.h
│
├── src/
│   ├── buzzer.cpp
│   ├── config.cpp
│   ├── display.cpp
│   ├── event_log.cpp
│   ├── main.cpp
│   ├── rtc_time.cpp
│   ├── sensors.cpp
│   ├── settings.cpp
│   ├── tamper.cpp
│   └── webui.cpp
│
├── lib/
│   ├── Adafruit APDS9960 Library/
│   ├── Adafruit BMP085 Library/
│   └── Adafruit MPU6050/
│
├── platformio.ini
└── README.md
```

---

# Build and upload

Install **PlatformIO** and open the `transit-logger` directory as the PlatformIO project.

The project targets an ESP32-compatible `esp32dev` board definition with the Arduino framework.

### Build

```bash
pio run
```

### Upload

```bash
pio run --target upload
```

### Serial monitor

```bash
pio device monitor
```

Serial communication is configured for:

```text
115200 baud
```

---

# Important configuration

Most hardware-dependent settings are in:

```text
include/config.h
```

Before deploying, verify:

- I2C wiring
- Button wiring and polarity
- MPU6050 interrupt wiring
- Battery-divider ratio
- Tamper foil calibration
- Motion sensitivity
- Logging interval
- Default Wi-Fi password
- Buzzer behaviour

The firmware currently uses the standard PlatformIO `default.csv` partition layout with LittleFS enabled.

---

## Libraries

The project uses the Arduino framework and the following libraries:

- Adafruit SSD1306
- Adafruit GFX Library
- Adafruit BusIO
- Adafruit Unified Sensor
- Adafruit APDS9960
- Adafruit BMP085 / BMP180
- Adafruit MPU6050
- ESP32 Preferences
- ESP32 Wi-Fi / WebServer / DNSServer / mDNS
- LittleFS

The APDS9960, BMP085 and MPU6050 libraries are included under the project's `lib/` directory.

---

## Notes

- The firmware is specifically matched to the current pin assignments and hardware configuration documented above.
- The APDS9960 is currently used only as an ambient-light sensor.
- The device spends most of its time in ESP32 deep sleep during logging.
- Thresholds and logging settings are stored in NVS.
- Event data is stored locally in LittleFS and can be downloaded through the Web UI.
- Time must currently be synchronised through the Web UI; there is no automatic NTP routine in the current firmware.
