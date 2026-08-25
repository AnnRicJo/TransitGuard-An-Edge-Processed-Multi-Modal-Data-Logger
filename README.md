---
publishDate: 2026-08-25T00:00:00Z
title: TransitGuard - An Edge-Processed Multi-Modal Data Logger
excerpt: An intelligent, low-power transit logging system built with the MYOSA board to track shocks, environmental extremes, and tamper events during high-value cargo transport.
image: ./assets/images/cover-image.jpg
tags:
  - ESP32
  - MYOSA
  - Edge-Computing
  - Sensor-Fusion
  - Supply-Chain
---
<p align="center">
  <img src="./assets/images/cover-image.jpeg" width="800"><br/>
  <i>TransitGuard: Edge-Processed Multi-Modal Data Logger for High-Value Cargo Monitoring</i>
</p>

> TransitGuard provides an immutable, timestamped audit trail of high-value cargo during transit using edge computing and multi-modal sensor fusion.

## Acknowledgements

Special thanks to Team MYOSA Sensors and the National Institute of Technology Calicut for supporting hardware access and resources for this project.

## Overview

Millions of dollars are lost annually due to the mishandling of fragile, high-value cargo during transit—ranging from sensitive electronics and semiconductor wafers to delicate prototyping equipment, and extending to time- and temperature-critical biological cargo such as donor organs, tissue samples, blood products, and vaccines. Current market solutions, such as chemical "shock-watch" stickers or standalone data-loggers, typically measure only a single variable, lack tamper-evidence, and fail to record the specific timestamp of an event—rendering them ineffective for comprehensive auditing, cold-chain compliance, or fault attribution when it matters most.

TransitGuard is a compact, battery-powered smart logger that leverages edge computing to track severe mechanical shocks, environmental extremes, and unauthorized access. For biological and medical cargo in particular, continuous temperature and pressure monitoring can flag a break in the cold chain (a critical concern for organs, tissue, blood, and vaccines, where even a short excursion outside the safe range can render the shipment non-viable), while shock detection flags mishandling that could damage a transport container, and tamper detection provides chain-of-custody assurance for sensitive or regulated biological material. By providing an immutable, timestamped audit trail of a package's journey, TransitGuard offers a robust, commercially viable solution to a critical vulnerability across both industrial and biomedical supply chains.

**Key features:**
* **Interrupt-Driven Edge Processing:** Operates in deep sleep and wakes via hardware interrupts (button, MPU6050 motion, capacitive tamper) or a configurable timer, keeping power draw to a minimum between events.
* **Multi-Modal Sensor Fusion:** Combines a 6-axis IMU, barometric pressure/temperature, and ambient light monitoring into a single fused reading on every logged event.
* **Capacitive Tamper Detection:** An aluminium-foil lining wired to the ESP32's native touch peripheral detects package opening or foil breach, with the tamper state latched in NVS so it survives a reset.
* **Onboard Web Dashboard:** The device hosts its own Wi-Fi access point and a browser-based dashboard for live sensor readings, threshold configuration, time sync, CSV download, and resetting for the next transit — no external app or cloud service required.
* **Local Readout & CSV Storage:** Displays live status on an OLED display and archives the full audit log directly on the ESP32's onboard flash (LittleFS) in `.CSV` format, downloadable through the dashboard.

## Demo / Examples

### **Hardware Assembly & Schematics**

The following images show the circuit design and progressive assembly of the TransitGuard enclosure, from initial module testing to the fully integrated package.

<p align="center">
  <img src="/assets/images/circuit-diagram.jpeg" width="800"><br/>
  <i>Circuit schematic showing the MYOSA ESP32 board, T856-C power module, and sensor bus</i>
</p>

<p align="center">
  <img src="/assets/images/internals.jpg" width="800"><br/>
  <i>Bench test setup and wiring arrangement of the sensing suite and processing electronics</i>
</p>

<p align="center">
  <img src="/assets/images/quarter-assembly.jpg" width="800"><br/>
  <i>Initial enclosure fitment and modular wiring integration</i>
</p>

<p align="center">
  <img src="/assets/images/half-assembly.jpg" width="800"><br/>
  <i>Intermediate assembly of the hardware stack and battery inside the custom housing</i>
</p>

<p align="center">
  <img src="/assets/images/lid-open.jpg" width="800"><br/>
  <i>TransitGuard enclosure opened to reveal the integrated electronics and internal capacitive seal</i>
</p>

<p align="center">
  <img src="/assets/images/full-pack.jpg" width="800"><br/>
  <i>TransitGuard fully assembled and sealed within the protective transport enclosure</i>
</p>

### **Web UI & Telemetry Dashboard**

<p align="center">
  <img src="/assets/images/webui-two.jpeg" width="800"><br/>
  <i>Onboard Web UI displaying live IMU readings, dynamic thresholds, and system health</i>
</p>

<p align="center">
  <img src="/assets/images/webui-one.jpeg" width="800"><br/>
  <i>Audit log management interface for downloading and parsing CSV telemetry records directly from LittleFS</i>
</p>

### **Videos**

<video controls width="100%">
  <source src=".
/assets/videos/transitguard-demo.mp4" type="video/mp4">
</video>
https://github.com/user-attachments/assets/3e953925-9568-4318-9980-43b475f0cb56

## Features (Detailed)

### **1. Interrupt-Driven Edge Shock & Motion Logging (MPU6050)**
Continuous data logging rapidly drains battery life and flash write cycles. TransitGuard configures the MPU6050 6-axis IMU to maintain a hardware motion interrupt on GPIO27. The ESP32 remains in deep sleep and wakes via an `EXT1` multi-pin bank configured with RTC pull-downs. Upon wake, it evaluates individual directional axes ($X, Y, Z$) and dynamic shock magnitude ($|a_{\text{magnitude}} - 1.0g|$) against configured limits before isolating I2C bus pins and returning to sleep.

### **2. Multi-Modal Environmental Monitoring (BMP180 & APDS9960)**
The BMP180 sensor monitors ambient cargo hold temperature and barometric pressure, with automatic multi-tier temperature fallback (BMP180 $\rightarrow$ MPU6050 internal $\rightarrow$ ESP32 internal) to ensure continuity. Breaches are edge-triggered (logged once upon entering an out-of-spec state) to prevent flash log overflow during prolonged temperature or pressure excursions.

### **3. Dual-Layer Tamper Detection & Hardware Latching**
Physical package integrity is enforced through two complementary mechanisms:
* **Capacitive Touch Seal:** An internal aluminium-foil lining connected to GPIO4 (`Touch0`) detects enclosure breach or tearing via native capacitive-touch wake. The tamper status latches permanently into Non-Volatile Storage (NVS) and survives power loss or reboot until authenticated reset.
* **Optical Detection & Status LED:** The APDS9960 catches light leaks if the enclosure is opened without touching the foil, while the status LED fast-blinks whenever a tamper event is active.

### **4. Local Diagnostic Display & Auditory Alerts (SSD1306 OLED + Buzzer)**
Handlers can inspect transit health without external devices:
* **Boot Diagnostic Screen:** Runs a persistent hardware check verifying sensor presence (MPU6050, BMP180, APDS9960) and battery voltage on power-up.
* **Field Info Mode:** Pressing the Display button renders a 5-second summary (battery, timestamp, tamper state, safety flags)[cite: 2]. If any threshold is breached, the integrated piezo buzzer emits continuous acoustic alert patterns while the screen is visible[cite: 2].

### **5. Onboard Web Dashboard & Telemetry Server**
Pressing the Mode button starts a SoftAP with captive DNS and mDNS (`http://transitlogger.local`), serving a zero-install dashboard:
* **Live Telemetry & Sparklines:** Streams live sensor snapshots, orientation leveling, and battery discharge trends via `/status`.
* **Dynamic Configuration:** Adjust shock sensitivity, directional $G$-force limits, tilt limits (pitch/roll), and logging intervals directly from the browser.
* **Browser Time Synchronization:** Synchronizes RTC epoch and timezone offset instantly from the connected browser.
* **Audit Trail Management:** Download the full raw `transit_log.csv` file stored in LittleFS, export filtered client-side CSVs, or clear storage for the next journey.

## Usage Instructions

1. Connect the 3.7V LiPo battery and charge via the onboard USB-C port.
2. Power on the device: the OLED presents the initial boot splash while peripherals initialize, followed by a continuous sensor diagnostic screen (validating APDS9960, MPU6050, BMP180, and battery state).
3. Press the **Mode (UI) button** to enter Web UI mode, connect to the device's Wi-Fi access point, and navigate to `http://transitlogger.local` to configure thresholds, logging intervals, and synchronize device time directly from the browser.
4. Return to logging mode (via the dashboard's "Go to sleep" / "Save/Start" button or the physical Mode button), place the unit inside the transit cargo package, and seal the enclosure.
5. In logging mode, the device remains in deep sleep and wakes via configured hardware interrupts (MPU shock, capacitive tamper, APDS proximity, periodic heartbeat, or manual button presses) to log edge-triggered events directly to flash.
6. Press the **Display Info button** in the field for a 5-second OLED health summary (battery, timestamp, tamper/threshold status) with active buzzer alerts if a breach is ongoing, or re-enter Web UI mode upon arrival to download `transit_log.csv`.

Sample CSV audit trail format:

```csv
timestamp,event_type,lux,temp_c,press_hpa,accel_x,accel_y,accel_z,mag_g,pitch,roll,orientation,batt_pct,note
2026-08-10 10:14:02,BOOT,0.0,24.5,1013.2,0.00,0.00,1.00,1.00,0.2,-0.4,FLAT,97,power-on
2026-08-10 11:02:15,MOTION,0.0,25.1,1012.8,2.14,0.87,3.92,4.82,5.1,-2.3,FLAT,95,mpu-motion-int
2026-08-10 11:45:00,TAMPER,342.1,26.0,1012.5,0.01,0.02,1.01,1.02,0.1,0.1,FLAT,94,touch-wake
2026-08-10 12:00:00,PERIODIC,0.0,25.8,1012.3,0.00,0.01,1.00,1.00,0.1,-0.2,FLAT,93,heartbeat
```

## Tech Stack

* **Embedded Processing:** ESP32-WROOM-32E (MYOSA Mini Kit) running FreeRTOS / Arduino Framework
* **Power Management:** T856C USB-C Power Shield, 3.7V LiPo battery, deep sleep with EXT0/EXT1 wake interrupts
* **Sensing Suite:** MPU6050 (6-Axis IMU), BMP180 (Temp / Pressure), APDS9960 (Ambient Light), Native Capacitive Touch
* **User Interface:** SSD1306 0.96" I2C OLED display & edge alert piezo buzzer
* **Local Web Interface:** On-chip SoftAP, HTTP Web Server, Captive DNS & mDNS (`transitlogger.local`)
* **Data Storage:** Flash-based LittleFS (`transit_log.csv`) & persistent NVS / RTC SRAM
* **Toolchain:** PlatformIO / C++

---
## Requirements / Installation

### Prerequisites
* [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation/index.html) or [VS Code with PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
* USB-to-UART bridge drivers for the ESP32 (e.g., CP210x or CH340)

### Setup & Flashing
Clone the repository and flash the firmware to the MYOSA ESP32 board:

```bash
# Clone the repository
git clone [https://github.com/your-username/transitguard.git](https://github.com/your-username/transitguard.git)
cd transitguard/transit-logger

# Build firmware
pio run

# Build and flash via USB
pio run --target upload

# Open serial monitor (115200 baud)
pio device monitor
```

## File Structure

```plaintext
/TransitGuard
├── assets/
│   ├── images/           # Hardware assembly, schematics, and UI photos
│   └── videos/           # Demo and functional test recordings
├── docs/                 # Extended documentation, notes, and pinout diagrams
├── firmware/
│   └── transit-logger/
│       ├── include/      # Modular C++ header files (buzzer, display, sensors, tamper, etc.)
│       ├── src/          # Source implementations and main application logic (main.cpp)
│       ├── lib/          # External hardware and sensor driver libraries
│       └── platformio.ini # PlatformIO environment, board, and dependency settings
├── LICENSE
└── README.md
```

## License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.

## Contribution Notes

Contributions, bug reports, and hardware improvements are welcome! Please open an issue to discuss proposed changes or submit a pull request on GitHub for any firmware enhancements or hardware revisions.
