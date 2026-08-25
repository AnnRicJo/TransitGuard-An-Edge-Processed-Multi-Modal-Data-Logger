# Transit Package Logger — Firmware

Firmware for the ESP32-based MYOSA Mini Kit used in the Transit Package Logger.

The firmware has two main operating modes:

* **Low-power logging (`MODE_LOGGING`):** The ESP32 spends most of its time in deep sleep and wakes to record events, periodic sensor readings, dynamic shocks, or enclosure tamper triggers.
* **Web UI (`MODE_WEBUI`):** The ESP32 creates its own Wi-Fi access point and hosts a browser dashboard for viewing real-time sensor data, modifying directional and environmental thresholds, zero-calibrating the IMU, downloading the CSV log, and preparing the device for the next transit session.

The firmware is written with PlatformIO and the Arduino framework.

---

## Hardware & Peripheral Interfaces

| Component | Interface / Address | Purpose |
|---|---|---|
| MYOSA Mini Kit / ESP32-WROOM-32E | — | Main microcontroller |
| SSD1306 OLED (128x64) | I2C `0x3C` | Real-time status, diagnostics, and package monitoring |
| APDS9960 | I2C `0x39` | Ambient-light measurement |
| MPU6050 | I2C `0x69` (AD0 High) | 6-DoF motion/shock detection and orientation estimation |
| BMP180 | I2C `0x77` | Barometric pressure and primary temperature sensing |
| Passive piezo buzzer | GPIO 19 | Audible user feedback and repeating threshold alert beeps |
| Aluminium-foil tamper sensor | ESP32 Touch0 (`GPIO 4`) | Capacitive enclosure seal breach and cut-foil detection |
| LiPo battery | ADC1 (`GPIO 34`) via 2:1 divider | Voltage monitoring and state-of-charge calculation |
| Motherboard Status LED | GPIO 2 | System activity, Web UI mode, and tamper status indication |

### Sensor Subsystem Details

* **Ambient Light Sensing:** The APDS9960 operates with color integration enabled and gesture engines disabled, measuring clear-channel lux to detect box-opening events.
* **Adaptive Temperature Selection:** BMP180 barometric temperature is preferred; MPU6050 internal temperature is used as the secondary fallback; ESP32 internal core temperature (`temperatureRead()`) is used as the final fallback.
* **Motion & Tilt Engine:** The MPU6050 operates across $\pm 8\text{ g}$ accelerometer and $\pm 500^\circ/\text{s}$ gyroscope ranges with a $21\text{ Hz}$ low-pass filter and a $0.63\text{ Hz}$ high-pass filter for motion interrupt detection. Pitch, roll, and spatial package orientation (`TOP_UP`, `UPSIDE_DOWN`, `TILT_X_POS`, `TILT_X_NEG`, `TILT_Y_POS`, `TILT_Y_NEG`, `ANGLED`) are calculated on every sample.

---

## Detailed Pinout Declarations

| Signal / Constant | GPIO | Direction / Mode | Functional Notes |
|---|---:|---|---|
| `I2C_SDA_PIN` | `21` | Bidirectional | Shared I2C Data bus; isolated via RTC domain during deep sleep to prevent panel noise |
| `I2C_SCL_PIN` | `22` | Output | Shared I2C Clock bus (100 kHz); isolated prior to deep sleep |
| `BTN_MODE_PIN` | `32` | Input (RTC Pull-down) | Active-HIGH UI/Mode toggle button; joined to `EXT1` wake mask |
| `BTN_DISPLAY_PIN` | `33` | Input (RTC Pull-down) | Active-HIGH Display button; wakes OLED for 5s status screen in logging mode |
| `MPU_INT_PIN` | `27` | Input (RTC Pull-down) | Active-HIGH latched hardware motion interrupt; joined to `EXT1` wake mask |
| `APDS_INT_PIN` | `25` | Input (Pull-up) | Dedicated active-LOW open-drain interrupt line (`EXT0`) |
| `TOUCH_TAMPER_PIN`| `4` | Capacitive Touch0 (`T0`) | Connected to internal security foil lining; wakes device on touch pad trigger |
| `BATT_ADC_PIN` | `34` | Analog Input (ADC1) | Sensed through a 2:1 divider with 11dB attenuation |
| `BUZZER_PIN` | `19` | Digital Output | Bit-banged square-wave drive at 2700 Hz (resonant peak) |
| `STATUS_LED_PIN` | `2` | Digital Output | Visual status indicator (wake heartbeat, Web UI indicator, tamper fast blink) |

### Button Wiring & EXT1 Wake Requirement

Both physical buttons are wired **active-HIGH**:

```text
3.3V ──[ BUTTON ]── GPIO (32 / 33)
                      │
                   internal
                   pulldown
```

* Idle state reads `LOW`.
* Pressed state reads `HIGH`.

The ESP32 deep-sleep `EXT1` controller requires uniform polarity when grouping multiple wake sources (`ESP_EXT1_WAKEUP_ANY_HIGH`). Because the MPU6050 interrupt is configured as active-HIGH, the physical buttons share the active-HIGH configuration on the `EXT1` bank.

---

## Configuration Parameters & NVS Settings

All tunable thresholds and operational settings are persisted in non-volatile flash storage using ESP32 `Preferences`:

| Parameter Name | NVS Key | Default Value | Unit | Functional Significance |
|---|---|---|---|---|
| `apSsid` | — | `"TransitLogger-AP"` | — | SoftAP SSID broadcast during Web UI mode |
| `apPass` | — | `"12345678"` | — | WPA2 password for dashboard authentication |
| `threshLux` | `"lux"` | `50.0` | Lux | Ambient light threshold that flags an enclosure breach or box-opening event |
| `threshTempC` | `"temp"` | `45.0` | °C | Thermal safety limit for sensitive cargo; triggers alerts when exceeded |
| `motionThreshold` | `"motion"` | `20` (Clamped $\ge 25$) | LSB | MPU6050 hardware motion engine threshold required to assert `MPU_INT_PIN` |
| `loggingIntervalSec` | `"interval"` | `60` | Seconds | Periodic sleep timer interval for routine heartbeat CSV snapshots |
| `threshAccelX` | `"th_ax"` | `2.0` | g | Shock threshold limit along the X-axis before logging `EVT_MOTION` |
| `threshAccelY` | `"th_ay"` | `2.0` | g | Shock threshold limit along the Y-axis before logging `EVT_MOTION` |
| `threshAccelZ` | `"th_az"` | `2.5` | g | Shock threshold limit along the Z-axis before logging `EVT_MOTION` |
| `threshPitch` | `"th_pitch"`| `45.0` | Degrees | Maximum allowable tilt angle on pitch before raising an alert |
| `threshRoll` | `"th_roll"` | `45.0` | Degrees | Maximum allowable tilt angle on roll before raising an alert |
| `calibOffsetX` | `"cal_x"` | `0.0` | g | Level zero calibration offset compensation for X-axis |
| `calibOffsetY` | `"cal_y"` | `0.0` | g | Level zero calibration offset compensation for Y-axis |
| `calibOffsetZ` | `"cal_z"` | `0.0` | g | Level zero calibration offset compensation for Z-axis (normalized to $+1.0\text{ g}$ upright) |

---

## Firmware Behaviour & Execution Flow

### 1. Power-on (Cold Boot)
* Draws the `Transit Logger` splash screen on the OLED panel.
* Initializes I2C bus peripherals, LittleFS filesystem, NVS settings, battery ADC, and tamper baseline.
* Logs an initial `EVT_BOOT` record to LittleFS and sounds a short buzzer confirmation.
* Runs a continuous sensor diagnostics screen showing live probe state (`OK/ACTIVE` or `NOT FOUND`) for MPU6050, BMP180, and APDS9960 along with live battery metrics.
* Blocks until the user presses the **Mode button**, transitioning the unit directly into Web UI mode.

### 2. Low-Power Logging Mode (`MODE_LOGGING`)
Between logging cycles, the OLED display is turned off (`SSD1306_DISPLAYOFF`), status LEDs are turned off, and the ESP32 enters deep sleep with wakeup sources armed:
* **Timer Wakeup:** Fires every `loggingIntervalSec` to record regular heartbeat entries.
* **MPU6050 Shock/Motion Wakeup (`EXT1`):** Wakes the chip when dynamic shock or directional acceleration limits are exceeded.
* **Capacitive Tamper Wakeup (`TouchPad`):** Wakes the chip if foil capacitance shifts beyond the calibration margin.
* **Display Button (`EXT1`):** Wakes the OLED for **5 seconds** (`LOWPOWER_DISPLAY_DURATION_MS`), displaying battery percentage, wall-clock time, safety status, and active breach flags. If tamper or threshold limits are breached while the display is awake, the buzzer sounds continuous repeating alarm beeps.
* **Mode Button (`EXT1`):** Wakes the system and switches state to `MODE_WEBUI`.

---

## Tamper Sensing & Latching

Tamper sensing utilizes native ESP32 capacitive touch sensing on `GPIO 4` (`Touch0`) connected to the package's internal aluminum foil lining:
* **Baseline Calibration:** A 10-sample baseline average is taken on initial boot and stored in NVS (`tamper` namespace).
* **Deviation Detection:** Any delta greater than `TAMPER_ALLOWED_DELTA` (default: 30 counts) latches the tamper state to `true` in flash.
* **NVS Latching:** Tamper evidence survives resets, reboots, and complete battery disconnects, remaining permanently flagged until acknowledged and cleared via the Web UI dashboard.

---

## Event Logging & CSV Specification

Events are written to `/events.csv` on the LittleFS partition.

### CSV Schema
```csv
timestamp,event_type,lux,temp_c,press_hpa,accel_x,accel_y,accel_z,mag_g,pitch,roll,orientation,batt_pct,note
```

### Event Type Definitions
* `EVT_BOOT` (0): Cold boot initialization event.
* `EVT_PERIODIC` (1): Routine heartbeat record triggered by sleep timer.
* `EVT_PROXIMITY` (2): Proximity interrupt event.
* `EVT_LIGHT_THRESHOLD` (3): Light ingress event exceeding lux threshold.
* `EVT_GESTURE` (4): Optical gesture trigger.
* `EVT_TEMP_THRESHOLD` (5): Temperature limit exceeded.
* `EVT_TAMPER` (6): Enclosure security foil capacitance breach.
* `EVT_MODE_CHANGE` (7): State transitions between Logging and Web UI modes.
* `EVT_BUTTON_DISPLAY` (8): Manual package status check via display button.
* `EVT_LOG_RESET` (9): Flash log cleared for a new transit deployment.
* `EVT_LOW_BATTERY` (10): Battery voltage dropped to $\le 10\%$.
* `EVT_MOTION` (11): Dynamic shock or directional acceleration threshold breach.

*Note: Threshold events are edge-triggered to prevent duplicate consecutive logs during sustained alarm states.*

---

## Interactive Web UI Dashboard

When switched into `MODE_WEBUI`, the ESP32 activates SoftAP mode, starts an internal DNS captive portal, and advertises via mDNS at `http://transitlogger.local`:

* **Live Telemetry Stream:** Asynchronous updates for ambient lux, temperature, barometric pressure, estimated altitude, 3-axis acceleration, dynamic shock ($g$), pitch, roll, spatial orientation, battery state, and tamper status.
* **Real-Time Data Visualizations:** Dynamic SVG sparkline graphs for telemetry channels.
* **Event Log Table:** Integrated filter, multi-column sorting, raw CSV download (`/download`), and client-side filtered CSV export.
* **System Actions & Calibration:** Zero Level Calibration averages 30 accelerometer readings; Time Synchronization syncs browser epoch to ESP32 RTC; Tamper Reset clears NVS tamper state; Transit Log Reset wipes stored CSV records; Arm Sleep commits updated parameters and drops back into `MODE_LOGGING`.

---

## Status LED Indication (`GPIO 2`)

| Device State | Status LED Behavior |
|---|---|
| Deep-Sleep Logging (`MODE_LOGGING`) | OFF (minimizes power consumption) |
| Wake / Logging Pulse | Brief 25ms flash on event recording |
| Web UI Active (`MODE_WEBUI`) | Solid ON |
| Tamper Latched | Fast continuous blinking (150ms period) |

---

## Project File Structure

* `include/buzzer.h` & `src/buzzer.cpp`: Bit-banged piezo tone driver definitions and direct GPIO pulse implementation.
* `include/config.h` & `src/config.cpp`: Pin assignments, timing macros, wake masks, thresholds, and event string conversion.
* `include/display.h` & `src/display.cpp`: SSD1306 OLED interface, glyph rendering, and screen layout methods.
* `include/event_log.h` & `src/event_log.cpp`: LittleFS CSV event log manager class and file operations.
* `include/rtc_time.h` & `src/rtc_time.cpp`: Wall-clock timekeeper and uptime string formatting.
* `include/sensors.h` & `src/sensors.cpp`: Sensor reading structures, I2C driver integration, and math calculations.
* `include/settings.h` & `src/settings.cpp`: NVS configuration storage and retrieval manager via Preferences.
* `include/tamper.h` & `src/tamper.cpp`: Capacitive touch baseline calculation, threshold checks, and NVS latching.
* `include/webui.h` & `src/webui.cpp`: SoftAP, HTTP server, REST endpoints, and embedded dashboard HTML/JS.
* `src/main.cpp`: Setup routines, deep-sleep logic, wake interrupt service, and control loops.

---

## Build, Flash & Monitor

### Compile Firmware
```bash
pio run
```

### Upload to ESP32
```bash
pio run --target upload
```

### Serial Monitor (115200 Baud)
```bash
pio device monitor
```