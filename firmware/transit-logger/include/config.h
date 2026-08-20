#pragma once
/* =====================================================================
 *  config.h
 *  --------------------------------------------------------------
 *  ALL board-wiring / tunable constants live here. This is the file
 *  you edit if your wiring differs from the assumptions documented
 *  in the chat response (button pins, touch pin, ADC divider, etc).
 * =====================================================================
 */
#include <Arduino.h>

/* ---------------------------------------------------------------------
 *  I2C bus (shared by OLED, APDS9960, MPU6050, BMP180)
 * ------------------------------------------------------------------- */
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define I2C_CLOCK_HZ        100000UL

#define OLED_I2C_ADDR       0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

#define APDS9960_I2C_ADDR   0x39   /* used by Adafruit_APDS9960 internally */
#define MPU6050_I2C_ADDR    0x69   /* AD0 pin tied high on your board */
/* BMP180 has a single fixed I2C address (0x77) -- no #define needed,
 * Adafruit_BMP085 hardcodes it. */

/* ---------------------------------------------------------------------
 *  Buzzer -- a plain passive piezo buzzer wired directly to a GPIO
 *  (no driver module / no I2C expander). Being "passive" it needs an
 *  actual square wave, not just a DC high, to make noise -- buzzer.cpp
 *  bit-bangs that tone directly on this pin.
 *
 *  HARDWARE CAUTION: there's no transistor/driver stage and no series
 *  resistor in this design -- the ESP32 GPIO is expected to source/sink
 *  the piezo's drive current directly. ESP32 GPIOs are rated for
 *  ~12mA continuous / ~40mA absolute max per pin. Most small piezo
 *  elements draw well under that, but confirm your specific buzzer's
 *  datasheet before relying on this; if in doubt, add a ~100 ohm
 *  series resistor between BUZZER_PIN and the piezo to guarantee the
 *  GPIO is never asked to exceed its rated current.
 * ------------------------------------------------------------------- */
#define BUZZER_PIN           19
#define BUZZER_TONE_HZ        2700   /* close to a typical piezo's resonant peak */

/* ---------------------------------------------------------------------
 *  Buttons
 *  BTN_MODE_PIN    : "UI button" -- short press -> toggles
 *                    LOGGING/low-power <-> WEB_UI mode
 *  BTN_DISPLAY_PIN : "display button" -- short press -> wakes OLED for
 *                    5s (battery/time/safety status). Only meaningful
 *                    in low-power/logging mode; inert in Web UI mode.
 *
 *  IMPORTANT WIRING NOTE: these MUST be wired active-HIGH -- button
 *  between the GPIO and 3.3V, using the ESP32's internal pulldown
 *  (idle = LOW, pressed = HIGH). This is not a style choice: classic
 *  ESP32 deep-sleep multi-pin wakeup (EXT1) only supports either
 *  "wake if ALL selected pins are low" or "wake if ANY selected pin
 *  is high" -- never a per-pin mix. Since the MPU6050 interrupt on
 *  this same wake bank is active-HIGH by hardware default, the
 *  buttons have to be active-HIGH too so we can use ANY_HIGH mode.
 *  If you already wired them to GND, either re-wire them to 3.3V, or
 *  add an external inverter -- don't just flip BUTTON_ACTIVE_LOW below
 *  without also fixing the wiring, or wakeup won't behave as expected.
 * ------------------------------------------------------------------- */
#define BTN_MODE_PIN        32
#define BTN_DISPLAY_PIN     33
#define BUTTON_ACTIVE_LOW   0        /* 0 = active-HIGH wiring, see note above */
#define BUTTON_DEBOUNCE_MS  40
#define BUTTON_LONG_PRESS_MS 900

/* Small helper so the rest of the code never hardcodes polarity */
static inline bool buttonIsPressed(uint8_t pin) {
    return BUTTON_ACTIVE_LOW ? (digitalRead(pin) == LOW)
                              : (digitalRead(pin) == HIGH);
}

/* ---------------------------------------------------------------------
 *  Extra-strict confirmation used right after a deep-sleep EXT1 wake,
 *  before we trust that a wake was really caused by a button (and not
 *  by electrical noise / a floating pin momentarily crossing the
 *  input threshold). A wake glitch that's ONLY seen for one sample is
 *  exactly what produces "screen flickers on its own" symptoms -- the
 *  chip wakes for a few ms, thinks a button is down, flashes the OLED,
 *  goes back to sleep, and repeats. Requiring the pin to read pressed
 *  on every one of several samples spread over BUTTON_DEBOUNCE_MS
 *  filters that out. If flicker persists after this, it's a wiring
 *  issue (see the active-HIGH note above) rather than firmware.
 * ------------------------------------------------------------------- */
#define BUTTON_CONFIRM_SAMPLES      6
#define BUTTON_CONFIRM_SAMPLE_GAP_MS 5

/* ---------------------------------------------------------------------
 *  APDS9960 interrupt pin -> open-drain, active-LOW by hardware (can't
 *  be inverted), so it's woken via its own dedicated EXT0 source
 *  rather than sharing the EXT1 bank with the buttons/MPU6050.
 * ------------------------------------------------------------------- */
#define APDS_INT_PIN        25

/* ---------------------------------------------------------------------
 *  MPU6050 6-axis accelerometer/gyro interrupt (motion/shock detect).
 *  Left at its default push-pull, active-HIGH polarity so it can share
 *  the EXT1 ANY_HIGH wake bank with the buttons.
 * ------------------------------------------------------------------- */
#define MPU_INT_PIN          27
#define MOTION_THRESHOLD_DEFAULT  8    /* 1=very sensitive .. 255=insensitive,
                                               raw units per Adafruit_MPU6050;
                                               tune on the bench with the box's
                                               actual packing/cushioning */
#define MPU_MOTION_DURATION_SAMPLES   20   /* consecutive samples above threshold
                                               before the interrupt latches */

/* ---------------------------------------------------------------------
 *  Capacitive-touch tamper sense pin (connected to the aluminium foil
 *  lining the inner box edges). Uses the ESP32 native touch peripheral.
 * ------------------------------------------------------------------- */
#define TOUCH_TAMPER_PIN    4    /* GPIO4 = Touch0 */
#define TOUCH_TAMPER_CHANNEL T0

/* ---------------------------------------------------------------------
 *  Battery voltage sense (ST6845-C has NO digital telemetry, so we read
 *  battery voltage ourselves through a resistor divider into an
 *  ADC1 input-only pin). Adjust DIVIDER_RATIO to match your resistors.
 *  DIVIDER_RATIO = (R1+R2)/R2   e.g. two equal 100k resistors -> 2.0
 * ------------------------------------------------------------------- */
#define BATT_ADC_PIN         34
#define BATT_DIVIDER_RATIO   2.0f
#define BATT_ADC_VREF        3.3f
#define BATT_FULL_VOLTAGE    4.2f
#define BATT_EMPTY_VOLTAGE   3.3f

/* ---------------------------------------------------------------------
 *  Status LED already present on the MYOSA motherboard, wired to
 *  GPIO2. Meaning of each state (see updateStatusLed() in main.cpp):
 *    OFF          -> MODE_LOGGING, about to deep-sleep (idle transit)
 *    brief blink  -> chip just woke up / logged an event
 *    solid ON     -> MODE_WEBUI (AP + dashboard active)
 *    fast blink   -> tamper latched (either mode)
 * ------------------------------------------------------------------- */
#define STATUS_LED_PIN       2

/* ---------------------------------------------------------------------
 *  Timings
 * ------------------------------------------------------------------- */
#define LOWPOWER_DISPLAY_DURATION_MS 5000UL /* low-power-mode "display" button
                                                screen (battery/time/safety
                                                status) -- exactly 5s per spec */
#define LOWPOWER_DISPLAY_REFRESH_MS  200UL  /* how often that screen + the
                                                continuous breach buzzer are
                                                re-serviced during the 5s */
#define SENSOR_STATUS_REFRESH_MS   800UL    /* post-boot "sensor status"
                                                screen refresh cadence, shown
                                                continuously until the UI
                                                (mode) button is pressed */
#define WEBUI_INFO_REFRESH_MS      1000UL   /* Web UI mode OLED screen
                                                (URL/mode/battery/time)
                                                refresh cadence */
#define STATUS_LED_WAKE_BLINK_MS   25UL     /* brief heartbeat flash on wake */
#define STATUS_LED_TAMPER_BLINK_MS 150UL    /* fast-blink half-period when
                                                tamper is latched */
#define DEFAULT_LOGGING_INTERVAL_SEC 600UL  /* default time between logged
                                                sensor-data rows, user-
                                                configurable from the Web UI;
                                                also used as the deep-sleep
                                                timer-wake fallback so the
                                                log always has a heartbeat
                                                point even with no interrupt */
#define MIN_LOGGING_INTERVAL_SEC   30UL
#define MAX_LOGGING_INTERVAL_SEC   86400UL
#define TAMPER_POLL_INTERVAL_MS    2000UL

/* ---------------------------------------------------------------------
 *  Buzzer patterns (ms on / ms off), used for feedback + alerts.
 * ------------------------------------------------------------------- */
#define BEEP_SHORT_MS        60
#define BEEP_ALERT_ON_MS     150
#define BEEP_ALERT_OFF_MS    100
#define BEEP_ALERT_REPEATS   3

/* ---------------------------------------------------------------------
 *  Wi-Fi Access Point (Web UI mode) defaults — overridable via web UI
 *  and stored in NVS afterwards.
 * ------------------------------------------------------------------- */
#define DEFAULT_AP_SSID_PREFIX   "TransitLogger-"
#define DEFAULT_AP_PASSWORD      "logger1234"   /* >= 8 chars, change me! */
#define WEBSERVER_PORT           80

/* ---------------------------------------------------------------------
 *  Event log file (LittleFS)
 * ------------------------------------------------------------------- */
#define LOG_FILE_PATH        "/events.csv"
#define LOG_CSV_HEADER       "timestamp,event_type,value1,value2,value3,note\n"

/* ---------------------------------------------------------------------
 *  Default alert thresholds (all overridable from the Web UI, stored
 *  in NVS / Preferences so they survive resets).
 * ------------------------------------------------------------------- */
#define DEFAULT_THRESH_LUX        800.0f   /* ambient light, lux */
#define DEFAULT_THRESH_PROX       180.0f   /* proximity 0-255   */
#define DEFAULT_THRESH_TEMP_C     45.0f    /* deg C              */

/* Operating modes persisted across deep sleep */
enum OperatingMode : uint8_t {
    MODE_LOGGING = 0,
    MODE_WEBUI   = 1
};

/* Not a persisted mode of its own -- just the label used in comments /
 * log notes for the continuous post-boot "sensor status" screen that
 * runs (awake, no deep sleep) between a fresh power-on and the first
 * time the UI/mode button is pressed. See runInitialSensorStatusLoop()
 * in main.cpp. */

/* Event type codes written into the CSV log */
enum EventType : uint8_t {
    EVT_BOOT            = 0,
    EVT_PERIODIC        = 1,
    EVT_PROXIMITY       = 2,
    EVT_LIGHT_THRESHOLD = 3,
    EVT_GESTURE         = 4,
    EVT_TEMP_THRESHOLD  = 5,
    EVT_TAMPER          = 6,
    EVT_MODE_CHANGE     = 7,
    EVT_BUTTON_DISPLAY  = 8,
    EVT_LOG_RESET       = 9,
    EVT_LOW_BATTERY     = 10,
    EVT_MOTION          = 11   /* MPU6050 shock/motion interrupt */
};

const char *eventTypeToStr(uint8_t t);
