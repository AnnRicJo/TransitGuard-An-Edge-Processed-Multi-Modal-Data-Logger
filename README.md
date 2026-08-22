---
publishDate: 2026-08-25T00:00:00Z
title: TransitGuard - Edge-Processed Multi-Modal Data Logger
excerpt: An intelligent, low-power transit logging system built with the MYOSA board to track shocks, environmental extremes, and tamper events during high-value cargo transport.
image: cover-image.jpg
tags:
  - ESP32
  - MYOSA
  - Edge-Computing
  - Sensor-Fusion
  - Supply-Chain
---

TransitGuard provides an immutable, timestamped audit trail of high-value cargo during transit using edge computing and multi-modal sensor fusion.

## Acknowledgements

Special thanks to Team MYOSA Sensors and the National Institute of Technology Calicut for supporting hardware access and resources for this project.

## Overview

Millions of dollars are lost annually due to the mishandling of fragile, high-value cargo during transit—ranging from sensitive electronics and semiconductor wafers to delicate prototyping equipment. Current market solutions, such as chemical "shock-watch" stickers, only measure a single variable and fail to record the specific timestamp of an event, rendering them ineffective for comprehensive auditing or fault attribution.

**TransitGuard** is a compact, battery-powered smart logger that leverages edge computing to track severe mechanical shocks, environmental extremes, and unauthorized access. By providing an immutable, timestamped audit trail of a package's journey, it offers a robust, commercially viable solution to a critical supply chain vulnerability.

**Key features:**
* **Interrupt-Driven Edge Processing:** Operates in deep sleep and wakes via hardware interrupts (button, MPU6050 motion, capacitive tamper) or a configurable timer, keeping power draw to a minimum between events.
* **Multi-Modal Sensor Fusion:** Combines a 6-axis IMU, barometric pressure/temperature, and ambient light monitoring into a single fused reading on every logged event.
* **Capacitive Tamper Detection:** An aluminium-foil lining wired to the ESP32's native touch peripheral detects package opening or foil breach, with the tamper state latched in NVS so it survives a reset.
* **Onboard Web Dashboard:** The device hosts its own Wi-Fi access point and a browser-based dashboard for live sensor readings, threshold configuration, time sync, CSV download, and resetting for the next transit — no external app or cloud service required.
* **Local Readout & CSV Storage:** Displays live status on an OLED display and archives the full audit log directly on the ESP32's onboard flash (LittleFS) in `.CSV` format, downloadable through the dashboard.

# Hardware Assembly

The following images show the progressive assembly of the TransitGuard enclosure, from the initial hardware integration to the completed package-ready unit.

### 1. Full Assembly

<p align="center">
  <img src="/assets/images/full_pack.jpg" width="450"><br/>
  <i>TransitGuard fully assembled within the protective transport enclosure</i>
</p>

### 2. Half Assembly

<p align="center">
  <img src="/assets/images/half_assembly.jpg" width="450"><br/>
  <i>Partial assembly of the TransitGuard enclosure showing component placement</i>
</p>

### 3. Internal Electronics

<p align="center">
  <img src="/assets/images/internals.jpg" width="450"><br/>
  <i>Internal arrangement of the TransitGuard sensing, processing, and power electronics</i>
</p>

### 4. Lid Open

<p align="center">
  <img src="/assets/images/lid_open.jpg" width="450"><br/>
  <i>TransitGuard enclosure opened to expose the internal electronics and sensor assembly</i>
</p>

### 5. Quarter Assembly

<p align="center">
  <img src="https://github.com/AnnRicJo/TransitGuard-An-Edge-Processed-Multi-Modal-Data-Logger/blob/main/assets/images/quarter%20assembly.jpg" width="450"><br/>
  <i>Early-stage enclosure assembly showing the integration of the TransitGuard hardware</i>
</p>

## Demo / Examples

### **Images**

<p align="center">
  <img src="/assets/images/transitguard/hardware-setup.jpg" width="800"><br/>
  <i>TransitGuard hardware mounted inside the protective transport package</i>
</p>

<p align="center">
  <img src="/assets/images/transitguard/circuit-schematic.jpg" width="800"><br/>
  <i>Circuit schematic showing the MYOSA board, TP4056 power shield, and 1N5819 Schottky protection diodes</i>
</p>

### **Videos**

<video controls width="100%">
  <source src="/transitguard-demo.mp4" type="video/mp4">
</video>

## Features (Detailed)

### **1. Interrupt-Driven Edge Shock Logging (MPU6050)**
Continuous data logging rapidly consumes battery power and memory. TransitGuard configures the MPU6050 6-axis IMU to maintain a hardware motion/shock interrupt on GPIO27. The ESP32 remains in deep sleep and only wakes when the configured motion threshold is breached (or a button is pressed, or the periodic timer fires) via an EXT1 multi-pin wake bank, then reads and fuses all sensors and logs the peak acceleration magnitude, orientation, and timestamp before returning to sleep.

### **2. Environmental Monitoring (BMP180)**
The onboard BMP180 sensor tracks ambient temperature and atmospheric pressure. Temperature is selected automatically from the best available source: the BMP180 first, the MPU6050's internal temperature sensor if the BMP180 isn't detected, and the ESP32's internal sensor as a last-resort fallback. This monitors for dangerous freezing or overheating in cargo holds and helps flag rapid pressure changes that could indicate rough handling of sealed packaging.

### **3. Tamper Detection (Capacitive Touch + APDS9960)**
An aluminium-foil lining inside the sealed enclosure is wired to the ESP32's native capacitive-touch peripheral (GPIO4 / Touch0). After an initial baseline calibration, a significant deviation from that baseline is treated as tampering and the event is latched in NVS — it survives a reset or battery pull until explicitly cleared from the dashboard. The APDS9960 complements this by watching for sudden ambient-light spikes, catching cases where the box is opened without disturbing the foil.

### **4. Local Health Check Display (SSD1306 OLED)**
The integrated SSD1306 OLED gives cargo handlers a quick, tool-free status readout: a sensor-detection screen right after power-on, a low-power info screen (battery, time, tamper/threshold alerts) on a short button press, and a live status screen while the Web UI is active.

### **5. Onboard Web Dashboard**
A short press of the mode button switches the device into Web UI mode: it starts its own Wi-Fi access point and a browser dashboard reachable at `http://transitlogger.local`. From there, handlers can view live sensor data, adjust alert thresholds and the logging interval, sync the device clock, download the full CSV audit log, and reset the log for the next shipment — all without any external app or cloud connection.

## Usage Instructions

1. Charge the 3.7V LiPo battery using the onboard TP4056 USB-C port.
2. Power on the system; the OLED shows the boot screen, then the sensor-detection status screen.
3. Press the **Mode/UI button** to enter Web UI mode, connect to the device's Wi-Fi access point, and open `http://transitlogger.local` to sync the device clock and review/adjust thresholds. (The firmware doesn't auto-sync over NTP — time is set from the browser through the dashboard.)
4. Return to logging mode (via the dashboard's "Go to sleep now" button or the physical Mode button), place the unit inside the shipment package, and seal the enclosure.
5. Upon arrival, read the summary on the OLED, or re-enter Web UI mode and download `transit_log.csv` from the dashboard.

Sample CSV audit trail format:

```csv
timestamp,event_type,lux,temp_c,press_hpa,accel_x,accel_y,accel_z,mag_g,pitch,roll,orientation,batt_pct,note
2026-08-10 10:14:02,BOOT,0.0,24.5,1013.2,0.00,0.00,1.00,1.00,0.2,-0.4,FLAT,97,
2026-08-10 11:02:15,MOTION,0.0,25.1,1012.8,2.14,0.87,3.92,4.82,5.1,-2.3,FLAT,95,
2026-08-10 11:45:00,TAMPER,342.1,26.0,1012.5,0.01,0.02,1.01,1.02,0.1,0.1,FLAT,94,foil-deviation
```

Deep-sleep wake configuration (buttons + MPU6050 share an EXT1 wake bank, capacitive tamper and a periodic timer are armed separately):

```cpp
// Buttons (active-HIGH) + MPU6050 motion interrupt share one EXT1 wake bank
static uint64_t buildExt1Mask() {
    return (1ULL << BTN_MODE_PIN) | (1ULL << BTN_DISPLAY_PIN) | (1ULL << MPU_INT_PIN);
}

esp_sleep_enable_ext1_wakeup(buildExt1Mask(), ESP_EXT1_WAKEUP_ANY_HIGH);
esp_sleep_enable_timer_wakeup((uint64_t)loggingIntervalSec * 1000000ULL);
Tamper.armWakeup();
esp_deep_sleep_start();
```

## Tech Stack

* MYOSA Mini Kit (ESP32-WROOM-32E based)
* MPU6050 6-Axis IMU & BMP180 Pressure/Temperature Sensor
* APDS9960 Ambient Light Sensor & SSD1306 OLED
* Capacitive-touch tamper sensing (ESP32 native touch peripheral)
* C++ / Arduino Framework / PlatformIO
* TP4056 Power Management, ESP32 LittleFS Event Logging
* ESP32 Wi-Fi Access Point, WebServer, DNS Server & mDNS (onboard dashboard)

## Requirements / Installation

To build and flash the firmware to the MYOSA ESP32 board using PlatformIO CLI:

```bash
# Clone the repository
git clone https://github.com/your-username/transitguard.git
cd transitguard/transit-logger

# Build and upload firmware
pio run --target upload
```

## File Structure

```plaintext
/transit-logger
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
├── lib/
│   ├── Adafruit APDS9960 Library/
│   ├── Adafruit BMP085 Library/
│   └── Adafruit MPU6050/
├── platformio.ini
└── README.md
```

## License

This project is released under the MIT License.

## Contribution Notes

Contributions and improvements are welcome! Please open an issue or submit a pull request on GitHub for any firmware enhancements or hardware shield revisions.
