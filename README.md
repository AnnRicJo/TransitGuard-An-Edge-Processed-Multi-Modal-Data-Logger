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
* **Interrupt-Driven Edge Processing:** Operates in deep sleep and wakes via hardware interrupts to log specific G-force shock violations.
* **Multi-Modal Sensor Fusion:** Combines 6-axis IMU, barometric pressure, temperature, and ambient light monitoring.
* **Tamper & Breach Detection:** Utilizes light sensing and capacitive breach detection to log package opening events.
* **Local Readout & CSV Storage:** Displays live status on an OLED display and archives full audit logs to a MicroSD card in `.CSV` format.
# Hardware Assembly

The following images show the progressive assembly of the TransitGuard enclosure, from the initial hardware integration to the completed package-ready unit.

<p align="center">
  <img src="/assets/images/full_pack.jpg" width="800"><br/>
  <i>TransitGuard fully assembled within the protective transport enclosure</i>
</p>

<p align="center">
  <img src="/assets/images/half_assembly.jpg" width="800"><br/>
  <i>Partial assembly of the TransitGuard enclosure showing component placement</i>
</p>

<p align="center">
  <img src="/assets/images/internals.jpg" width="800"><br/>
  <i>Internal arrangement of the TransitGuard sensing, processing, and power electronics</i>
</p>

<p align="center">
  <img src="/assets/images/lid_open.jpg" width="800"><br/>
  <i>TransitGuard enclosure opened to expose the internal electronics and sensor assembly</i>
</p>

<p align="center">
  <img src="/assets/images/quarter_assembly.jpg" width="800"><br/>
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
  <i>Circuit schematic showing MYOSA board, TP4056 power shield, and 1N5819 Schottky protection diodes</i>
</p>

### **Videos**

<video controls width="100%">
  <source src="/transitguard-demo.mp4" type="video/mp4">
</video>

## Features (Detailed)

### **1. Interrupt-Driven Edge Shock Logging (MPU6050)**
Continuous data logging rapidly consumes battery power and memory. TransitGuard configures the MPU6050 6-axis IMU to maintain an ultra-low-power impact threshold engine. The primary ESP32 remains in deep sleep until an impact breaches the designated G-force threshold, triggering a hardware interrupt pin to wake the board and capture the peak acceleration and timestamp.

### **2. Environmental & Aircraft Depressurization Monitoring (BMP180)**
The onboard BMP180 sensor tracks ambient temperature and atmospheric pressure at regular timer intervals. This monitors for dangerous freezing or overheating in cargo holds and detects rapid depressurization events in aircraft holds that could rupture air-sealed packaging.

### **3. Tamper & Enclosure Breach Detection (APDS9960)**
Installed inside a dark, sealed container, the APDS9960 ambient light sensor monitors for sudden illumination spikes. If an unauthorized individual opens the package during transit, the exact timestamp and light duration are permanently logged to the audit trail.

### **4. Local Health Check Display (SSD1306 OLED)**
Upon arrival at the destination, cargo handlers can view a quick system health report directly on the integrated SSD1306 OLED screen (displaying Maximum G-Force, Temperature Extremes, and Tamper Status) without needing external debug tools.

## Usage Instructions

1. Charge the 3.7V LiPo battery using the onboard TP4056 USB-C port.
2. Insert a FAT32 formatted MicroSD card into the custom shield slot.
3. Power on the system; the MYOSA board will initialize sensors, acquire NTP time sync via Wi-Fi (if available), and enter Deep Sleep mode.
4. Place the unit inside the shipment package and seal the enclosure.
5. Upon arrival, read the summary on the OLED screen or remove the MicroSD card to retrieve `audit_log.csv`.

Sample CSV audit trail format:

```csv
TIMESTAMP,EVENT_TYPE,G_FORCE_MAX,TEMP_C,PRESS_HPA,LIGHT_LUX
2026-08-10 10:14:02,INITIALIZATION,1.01G,24.5C,1013.2,0.0
2026-08-10 11:02:15,IMPACT_VIOLATION,4.82G,25.1C,1012.8,0.0
2026-08-10 11:45:00,TAMPER_DETECTED,1.02G,26.0C,1012.5,342.1
```

Firmware interrupt snippet:

```cpp
// ESP32 Wakeup interrupt handler for MPU6050 G-Force breach
void IRAM_ATTR onGForceExceeded() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(imuInterruptSemaphore, &xHigherPriorityTaskWoken);
}
```

## Tech Stack

* MYOSA Mini IoT Kit (ESP32 Gateway)
* MPU6050 6-Axis IMU & BMP180 Pressure Sensor
* APDS9960 Light/Proximity Sensor & SSD1306 OLED
* C++ / Arduino Framework / PlatformIO
* TP4056 Power Management & SPI MicroSD Logging

## Requirements / Installation

To build and flash the firmware to the MYOSA ESP32 board using PlatformIO CLI:

```bash
# Clone the repository
git clone https://github.com/your-username/transitguard.git
cd transitguard/firmware

# Build and upload firmware
pio run --target upload
```

## File Structure

```plaintext
/transitguard
│-- transitguard.md
│-- transitguard-demo.mp4
│-- cover-image.jpg
├── assets/
│   └── images/
│       └── transitguard/
│           ├── hardware-setup.jpg
│           └── circuit-schematic.jpg
├── firmware/
│   └── src/
│       └── main.cpp
└── README.md
```

## License

This project is released under the MIT License.

## Contribution Notes

Contributions and improvements are welcome! Please open an issue or submit a pull request on GitHub for any firmware enhancements or hardware shield revisions.
