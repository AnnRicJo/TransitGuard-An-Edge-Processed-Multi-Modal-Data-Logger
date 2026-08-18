# Transit Package Logger — MYOSA Mini Kit firmware

PlatformIO / Arduino-framework firmware for an ESP32-based (MYOSA Mini Kit)
transit-condition logger: deep-sleep event logging + an on-demand Wi-Fi
"Web UI" configuration/download mode, OLED status display, buzzer
feedback, and capacitive-touch tamper sensing on the box's foil lining.

## 1. Hardware on this build

| Module                | Chip      | I2C addr | Library used                     |
|------------------------|-----------|----------|-----------------------------------|
| OLED display           | SSD1306   | `0x3C`   | Adafruit_SSD1306 / GFX            |
| Light/Prox/Gesture     | APDS9960  | `0x39`   | Adafruit_APDS9960                 |
| 6-axis accel/gyro      | MPU6050   | `0x69`   | Adafruit_MPU6050 (AD0 tied high)  |
| Pressure/altitude/temp | BMP180    | `0x77`   | Adafruit_BMP085 (fixed address)   |
| Buzzer                 | plain passive piezo, direct GPIO | — | bit-banged tone, `buzzer.cpp` |

The ST6845-C charger module has **no digital interface** (LED-only
charge indication) — battery state is measured by the ESP32 itself via
an ADC + resistor divider.

### Temperature source

Three of these chips can report a temperature, so `sensors.cpp` picks
the most trustworthy one automatically, in this priority order:

1. **BMP180** — purpose-built ambient temp/pressure sensor, no
   significant self-heating. Used whenever present.
2. **MPU6050** — its temp sensor exists for gyro drift compensation,
   not ambient sensing, and sits next to an active gyro die, so it
   reads a bit high. Used only if BMP180 isn't found.
3. **ESP32 internal die sensor** — last-resort fallback; badly
   self-heated by the WiFi radio, especially in Web UI/AP mode.

## 2. Pin map

| Signal                         | GPIO | Notes                                  |
|---------------------------------|------|-----------------------------------------|
| I2C SDA / SCL                   | 21 / 22 | shared bus: OLED, APDS9960, MPU6050, BMP180 |
| Button — mode switch             | 32   | **active-HIGH**: wired to 3.3V, internal pulldown |
| Button — OLED display            | 33   | **active-HIGH**: wired to 3.3V, internal pulldown |
| APDS9960 INT (active low, open-drain) | 25 | own dedicated EXT0 wake source   |
| MPU6050 INT (active high, push-pull)  | 27 | shares EXT1 wake bank w/ buttons |
| Capacitive tamper foil           | 4    | native ESP32 touch (Touch0)             |
| Battery ADC (via 2:1 divider)    | 34   | ADC1, input-only pin                    |
| Buzzer (passive piezo, direct)   | 19   | bit-banged square wave, no driver IC    |
| Onboard blue LED                 | 2    | MYOSA motherboard default               |

None of these are strapping pins **except GPIO2** (the onboard LED). The
real ESP32 strapping-pin set is `0/2/5/12/15` — GPIO4 (tamper touch) is
*not* one of them, contrary to an earlier draft of this note. GPIO2
being a strapping pin is fine here because the LED is a fixed feature
of the MYOSA motherboard itself (not a wiring choice you're making),
and a plain LED-to-GPIO2 doesn't disturb the boot-mode sampling on
this board family. Just don't repurpose GPIO2 for anything that could
pull it hard high/low externally at reset (e.g. a strong pull-up from
another peripheral), or you risk intermittent boot failures.

### ⚠️ Why the buttons must be wired active-HIGH

Classic ESP32 silicon only offers two modes for waking on multiple
GPIOs at once (`EXT1`): *"wake if ALL selected pins are low"* or
*"wake if ANY selected pin is high"* — never a per-pin mix. The
MPU6050's interrupt line is active-HIGH by hardware default and shares
this same wake bank with your two buttons, so the buttons have to be
wired active-HIGH too (button between the GPIO and 3.3V, ESP32
internal pulldown so idle=LOW/pressed=HIGH) to use `ANY_HIGH` mode.
The APDS9960's interrupt is open-drain and can only ever be
active-LOW, so it doesn't share this bank at all — it gets its own
independent `EXT0` wake source instead. `BUTTON_ACTIVE_LOW` in
`config.h` reflects this; don't flip it without also re-wiring the
buttons, or wakeup will silently stop working as expected.

One caveat to test on your actual board: combining `EXT0` + `EXT1` +
touchpad + timer wakeup simultaneously is standard on modern ESP32
silicon revisions (rev 3, which is what you'll get on any board sold
today), but was restricted on very early rev-0/1 chips. Worth
confirming on first bring-up.

## 3. Firmware architecture

```
include/config.h     all pins + tunable constants (edit this first)
include/settings.h   \  thresholds (incl. motion sensitivity) + Wi-Fi
src/settings.cpp     /  creds, persisted in NVS
include/rtc_time.h   \  software RTC (uses ESP32's own RTC domain,
src/rtc_time.cpp     /  which survives deep sleep) + NTP sync
include/buzzer.h     \  direct-GPIO bit-banged tone for the passive
src/buzzer.cpp        /  piezo buzzer (no driver IC)
include/sensors.h    \  APDS9960 (lux/proximity/gesture), MPU6050
src/sensors.cpp      /  (accel/gyro/motion), BMP180 (pressure/altitude/
                         temp), battery ADC, best-temperature selection
include/tamper.h     \  capacitive tamper sense, latched in NVS so it
src/tamper.cpp        /  survives even a battery pull
include/event_log.h  \  CSV event log on LittleFS
src/event_log.cpp    /
include/display.h    \  SSD1306 OLED screens (post-boot sensor status,
src/display.cpp      /  web-ui info, low-power display-button screen)
include/webui.h      \  Wi-Fi AP + HTTP dashboard + captive-portal DNS
src/webui.cpp         /  (status, settings, CSV download, reset, tamper-clear)
src/main.cpp          orchestrates both modes + deep-sleep wake logic
```

### Mode 0 — Post-boot sensor status (cold power-on only)
On a true power-on (not a deep-sleep wake), the OLED shows a boot
splash, then a continuously-refreshing screen reporting whether each
sensor (MPU6050 / BMP180 / APDS9960) was recognised on the I2C bus and
is active, plus battery charge. This stays up indefinitely -- no
timeout -- until the UI (mode) button is pressed, which starts Web UI
mode.

### Mode 1 — Web UI (access point)
Started/stopped with the mode button. Brings up a Wi-Fi AP
(`TransitLogger-xxxxxx` / see `config.h` for default password --
**change it**), plus a captive-portal DNS server and mDNS name
(`http://transitlogger.local`) so you don't have to type the AP's IP.
The OLED shows a persistent screen: the URL to open, a "WEB UI MODE"
indicator, battery, and time. Serves a dashboard with:
- live sensor readouts (light/proximity/battery/temp/pressure/shock,
  polled via `/status` JSON every 3s)
- threshold settings form (lux / proximity / temperature / motion
  sensitivity) and the **logging interval** (seconds between recorded
  sensor-data rows)
- a "home Wi-Fi" field for briefly joining a known network (AP+STA) to
  fetch NTP time
- **Download CSV** button (streams `/events.csv` from LittleFS)
- **Reset for next transit** button (wipes the log, keeps thresholds)
- tamper banner + one-click acknowledge/clear

Hitting **Save & Start logging** commits the settings and immediately
drops the device into low-power (deep-sleep) mode -- same effect as
the separate "Go to sleep now" button or the physical mode button.

### Mode 2 — Logging (deep sleep / "low power mode")
Wakes on: APDS9960 interrupt (EXT0), either button OR the MPU6050
motion/shock interrupt (shared EXT1 bank), the tamper touch pad, or the
user-configured logging-interval timer (`loggingIntervalSec`, set from
the Web UI, default `DEFAULT_LOGGING_INTERVAL_SEC` = 600s). Data is
recorded at that interval, or immediately whenever a threshold
interrupt fires (motion/proximity), or on a lux/temperature threshold
crossing detected on wake -- then goes straight back to sleep, with no
OLED feedback, unless:
- the **UI (mode) button** was pressed: switches back into Web UI mode
  (same screen as above).
- the **display button** was pressed: shows battery, time, and the
  package's safety status (threshold breaches + tamper) for exactly
  5 seconds. If anything is breached, the buzzer sounds continuously
  for as long as the screen is up. The display button is only
  meaningful in this mode -- it's inert while Web UI mode is active.

Per your spec, the buzzer **only sounds a threshold-breach alert while
the OLED is actively showing something** -- silent background wakes
never buzz, even if a threshold was crossed. Button presses always get
a short confirmation beep regardless of mode.

## 4. Things you should double check / decide before flashing

1. **Motion/shock sensitivity** (`motionThreshold` in Settings, default
   `MPU_MOTION_THRESHOLD_DEFAULT = 8`) — this is very sensitive by
   datasheet convention (1=most sensitive). Tune it on the bench with
   the box's actual internal cushioning/packing material, or you'll
   get a flood of `MOTION` events from ordinary vehicle vibration.
   Adjustable live from the Web UI settings form.
2. **Logging interval** (`loggingIntervalSec` in Settings, default
   `DEFAULT_LOGGING_INTERVAL_SEC = 600` seconds) — how often a sensor
   snapshot is recorded even with no interrupt. Adjustable from the
   Web UI, clamped to `MIN_LOGGING_INTERVAL_SEC`..`MAX_LOGGING_INTERVAL_SEC`
   in `config.h`. A threshold breach or motion/proximity interrupt
   still logs immediately regardless of this interval.
3. **Battery divider ratio** (`BATT_DIVIDER_RATIO` in config.h) —
   set this to match whatever resistor divider you actually build
   between the battery and GPIO34; the default assumes a 2:1 (equal
   resistor) divider.
4. **Tamper sensitivity** (`TAMPER_ALLOWED_DELTA` in `tamper.cpp`) —
   touch readings are sensitive to foil geometry/humidity; calibrate
   on your actual assembled box and tune this margin.
5. **AP password** — change `DEFAULT_AP_PASSWORD` in config.h before
   deploying; the dashboard currently has no login of its own.
6. **LittleFS size** — bump `board_build.partitions` to a custom table
   if you expect very long transits with frequent events.
7. **Buzzer volume/tone** — `BUZZER_TONE_HZ` (2700 Hz default) is a
   generic guess at a piezo's resonant peak; sweep it on the bench and
   pick whatever your specific buzzer is loudest at.

## 5. No more typing the IP address

`webui.cpp` runs a captive-portal-style DNS server (`DNSServer`, port
53) alongside the HTTP server: it answers *every* hostname lookup with
the AP's own IP, and any unmatched HTTP path 302-redirects back to
`/`. In practice:

- Most phones/laptops will auto-pop the dashboard as a "sign in to
  network" prompt the moment they join the AP (same mechanism as
  hotel/airport Wi-Fi).
- If it doesn't auto-pop, just type anything into the address bar
  (`transit`, `x`, `logger.com`) and it lands on the dashboard.
- There's also a fixed mDNS name: `http://transitlogger.local`
  (works out of the box on iOS/macOS/Android/most Linux; older
  Windows needs Bonjour installed).

No extra library needed — `DNSServer` and `ESPmDNS` both ship with the
arduino-esp32 core already pulled in by `platformio.ini`.

## 6. Build & flash (PlatformIO)

```bash
pio run                     # build
pio run --target upload     # flash
pio device monitor           # serial log (115200 baud)
```

First boot formats LittleFS automatically and takes the initial
tamper-sensor calibration baseline — do this with the box in its
final sealed, untouched state.
