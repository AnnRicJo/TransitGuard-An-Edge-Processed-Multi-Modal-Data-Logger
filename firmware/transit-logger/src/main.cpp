/* =====================================================================
 *  Transit Package Logger — main.cpp
 *  ---------------------------------------------------------------
 *  Power-on sequence (true cold boot only -- RTC memory, and so
 *  g_bootCount, is reset by a power-on but survives deep sleep):
 *   1. Boot splash on the OLED while peripherals initialise.
 *   2. Continuous "sensor status" screen -- per-sensor recognised/
 *      active state (MPU6050/BMP180/APDS9960) + battery -- refreshed
 *      indefinitely until the UI (mode) button is pressed. See
 *      runInitialSensorStatusLoop().
 *   3. UI button press -> Web UI mode.
 *
 *  Two operating modes after that, toggled with BTN_MODE_PIN:
 *
 *   MODE_LOGGING ("low power mode"): the ESP32 spends almost all its
 *      time in deep sleep, OLED off. It wakes on: (a) the APDS9960
 *      proximity/gesture interrupt, (b) either button, (c) the
 *      capacitive tamper touch pad, (d) the MPU6050 motion/shock
 *      interrupt, or (e) the user-configured logging-interval timer
 *      -- logs one CSV row describing why it woke + current sensor
 *      snapshot, then goes straight back to sleep, UNLESS:
 *        - the display button was pressed: shows a 5-second battery/
 *          time/safety-status screen first, with continuous buzzer
 *          feedback for as long as a threshold or tamper is breached
 *          while that screen is up.
 *        - the UI (mode) button was pressed: switches to MODE_WEBUI
 *          instead of sleeping.
 *      The display button is only meaningful in this mode -- it's
 *      inert in MODE_WEBUI.
 *
 *   MODE_WEBUI: the ESP32 stays awake, runs as a Wi-Fi access point,
 *      and serves the settings/download dashboard. The OLED shows a
 *      persistent screen (URL to open, "web UI mode" indicator,
 *      battery, time). Hitting Save/Start on the dashboard (or the
 *      physical mode button) drops back into MODE_LOGGING / low
 *      power mode and deep sleep resumes.
 *
 *  See config.h for every pin assignment / tunable constant.
 * =====================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "config.h"
#include "settings.h"
#include "rtc_time.h"
#include "buzzer.h"
#include "sensors.h"
#include "tamper.h"
#include "event_log.h"
#include "display.h"
#include "webui.h"
#include "driver/rtc_io.h"

/* ---------------------------------------------------------------------
 *  State that must survive deep sleep lives in RTC memory.
 * ------------------------------------------------------------------- */
RTC_DATA_ATTR static OperatingMode g_mode = MODE_LOGGING;
RTC_DATA_ATTR static uint32_t      g_bootCount = 0;

/* ---------------------------------------------------------------------
 *  Forward declarations
 * ------------------------------------------------------------------- */
static void initCommonPeripherals();
static uint64_t buildExt1Mask();
static void armDeepSleepSources();
static void goToDeepSleep();

static void handleWakeAndLog();
static bool checkThresholdsAndLog(const SensorReadings &r, bool oledActive);
static void getThresholdBreachFlags(const SensorReadings &r, bool &luxBreached, bool &tempBreached);

static bool confirmButtonPressed(uint8_t pin);
static void runInitialSensorStatusLoop();   /* post-boot, blocks until UI button */
static void runLoggingModeActiveWindow();   /* brief window right after waking */
static void enterWebUiMode();
static void runWebUiLoop();
static void exitWebUiModeBackToLogging();

static void setStatusLed(bool on);
static void statusLedWakeBlink();
static void statusLedServiceTamperBlink(bool tampered);

/* ===================================================================== */
void setup() {
    Serial.begin(115200);
    delay(50);

    initCommonPeripherals();
    statusLedWakeBlink();   /* brief heartbeat flash so a live unit visibly
                              "did something" on every wake, without
                              leaving the LED burning power during sleep */

    bool firstBoot = (g_bootCount == 0);
    g_bootCount++;

    if (firstBoot) {
        Display.drawBootSplash();
        EventLog.logEvent(EVT_BOOT, 0, 0, 0, "power-on");
        BuzzerDev.beepShort();
        delay(1200);

        /* Continuous sensor-recognition + battery screen. Blocks here
         * by design until the UI (mode) button is pressed -- only runs
         * on a true cold power-on, never on a deep-sleep wake, since
         * g_bootCount (RTC memory) survives deep sleep but resets on
         * power-on. May set g_mode = MODE_WEBUI before returning. */
        runInitialSensorStatusLoop();
    }

    if (g_mode == MODE_WEBUI) {
        /* Either the UI button was just pressed above, or we were
         * already in Web UI mode before this reset/boot (e.g. a
         * watchdog reset while the dashboard was open) -- either way,
         * just (re)start it. */
        enterWebUiMode();
        return; /* loop() takes over servicing the web server */
    }

    /* ---- MODE_LOGGING path ---------------------------------------- */
    handleWakeAndLog();
    runLoggingModeActiveWindow();  /* may switch g_mode to WEBUI */

    if (g_mode == MODE_WEBUI) {
        enterWebUiMode();
        return;
    }

    /* Still in logging mode -> go straight back to sleep */
    goToDeepSleep();
    /* never reached */
}

void loop() {
    /* Only used while MODE_WEBUI is active; MODE_LOGGING never reaches
     * here because setup() ends in esp_deep_sleep_start(). */
    runWebUiLoop();
}

/* =====================================================================
 *  Common peripheral bring-up, used by both modes
 * ===================================================================== */
static void initCommonPeripherals() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_CLOCK_HZ);

    pinMode(BTN_MODE_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
    pinMode(BTN_DISPLAY_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    Settings.begin();
    RtcTime.begin();

    if (!BuzzerDev.begin()) {
        Serial.println(F("[main] WARNING: buzzer GPIO init failed"));
    }
    if (!Sensors.begin()) {
        Serial.println(F("[main] WARNING: no I2C sensors responded (APDS9960/MPU6050/BMP180)"));
    }
    if (!Display.begin()) {
        Serial.println(F("[main] WARNING: OLED (SSD1306 @0x3C) not responding"));
    }
    if (!EventLog.begin()) {
        Serial.println(F("[main] WARNING: event log filesystem mount failed"));
    }
    Tamper.begin();
}

/* =====================================================================
 *  Deep sleep wake-source configuration
 * ===================================================================== */
static uint64_t buildExt1Mask() {
    /* Buttons (wired active-HIGH, see config.h note) + MPU6050 motion
     * interrupt (active-HIGH by default) share this bank in ANY_HIGH
     * mode. The APDS9960 interrupt is open-drain active-LOW and can't
     * join this bank -- it gets its own EXT0 source instead. */
    return (1ULL << BTN_MODE_PIN) | (1ULL << BTN_DISPLAY_PIN) | (1ULL << MPU_INT_PIN);
}

static void armDeepSleepSources() {
    const DeviceSettings &s = Settings.get();

    // 1. Arm MPU Motion interrupt
    Sensors.armMotionInterrupt(s.motionThreshold);

    // 2. Configure RTC pull-downs for active-HIGH wakeup bank (Buttons + MPU)
    rtc_gpio_init((gpio_num_t)BTN_MODE_PIN);
    rtc_gpio_set_direction((gpio_num_t)BTN_MODE_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis((gpio_num_t)BTN_MODE_PIN);
    rtc_gpio_pulldown_en((gpio_num_t)BTN_MODE_PIN);

    rtc_gpio_init((gpio_num_t)BTN_DISPLAY_PIN);
    rtc_gpio_set_direction((gpio_num_t)BTN_DISPLAY_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis((gpio_num_t)BTN_DISPLAY_PIN);
    rtc_gpio_pulldown_en((gpio_num_t)BTN_DISPLAY_PIN);

    rtc_gpio_init((gpio_num_t)MPU_INT_PIN);
    rtc_gpio_set_direction((gpio_num_t)MPU_INT_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis((gpio_num_t)MPU_INT_PIN);
    rtc_gpio_pulldown_en((gpio_num_t)MPU_INT_PIN);

    // 3. Register EXT1 (ANY_HIGH) for Buttons and MPU
    esp_sleep_enable_ext1_wakeup(buildExt1Mask(), ESP_EXT1_WAKEUP_ANY_HIGH);

    // 4. Register periodic timer and capacitive touch
    esp_sleep_enable_timer_wakeup((uint64_t)s.loggingIntervalSec * 1000000ULL);
    Tamper.armWakeup();
}

static void goToDeepSleep() {
    // Clear pending interrupts
    Sensors.clearInterrupt();
    Sensors.clearMotionInterrupt();

    // Blank and power off the OLED panel cleanly
    Display.sleep();
    setStatusLed(false);

    armDeepSleepSources();

    Serial.println(F("[main] Entering deep sleep..."));
    Serial.flush();

    // Isolate I2C pins so line noise during power cut doesn't glitch the display
    rtc_gpio_isolate((gpio_num_t)I2C_SDA_PIN);
    rtc_gpio_isolate((gpio_num_t)I2C_SCL_PIN);

    esp_deep_sleep_start();
}

/* =====================================================================
 *  Status LED (GPIO2) -- see the state table in config.h.
 * ===================================================================== */
static void setStatusLed(bool on) {
    digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

static void statusLedWakeBlink() {
    setStatusLed(true);
    delay(STATUS_LED_WAKE_BLINK_MS);
    setStatusLed(false);
}

/* Call periodically from an already-running loop (never from the
 * logging-mode deep-sleep path) to fast-blink the LED while tamper is
 * latched, and restore it to whatever steady state it should have
 * otherwise (caller-supplied via 'on' is NOT used here -- Web UI mode
 * always wants solid-on as its "otherwise" state, so this only ever
 * gets called there). */
static void statusLedServiceTamperBlink(bool tampered) {
    static uint32_t lastToggle = 0;
    static bool ledOn = true;
    if (!tampered) {
        setStatusLed(true);  /* back to steady "Web UI active" */
        return;
    }
    if (millis() - lastToggle >= STATUS_LED_TAMPER_BLINK_MS) {
        lastToggle = millis();
        ledOn = !ledOn;
        setStatusLed(ledOn);
    }
}

/* =====================================================================
 *  MODE_LOGGING: handle the reason we just woke up
 * ===================================================================== */
static void handleWakeAndLog() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    SensorReadings r = Sensors.readAll();

    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0: {
            /* Dedicated wake source for the APDS9960's active-low,
             * open-drain interrupt line. */
            String gesture = Sensors.pollGesture();
            if (gesture.length() > 0) {
                EventLog.logEvent(EVT_GESTURE, r.ambientLux, r.proximity, 0, gesture.c_str());
            } else {
                EventLog.logEvent(EVT_PROXIMITY, r.proximity, r.ambientLux, 0, "apds-int");
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t status = esp_sleep_get_ext1_wakeup_status();
            if (status & (1ULL << MPU_INT_PIN)) {
                EventLog.logEvent(EVT_MOTION, r.accelMagnitude_g, 0, 0, "mpu-motion-int");
                Sensors.clearMotionInterrupt();
            }
            /* Button-triggered wakes are logged as part of
             * runLoggingModeActiveWindow()'s own button handling, not
             * here, to avoid double-logging. */
            break;
        }
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            EventLog.logEvent(EVT_TAMPER, Tamper.lastRawValue(), 0, 0, "touch-wake");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            EventLog.logEvent(EVT_PERIODIC, r.ambientLux, r.proximity, r.temperatureC, "heartbeat");
            break;
        default:
            /* power-on / reset -- already logged EVT_BOOT in setup() */
            break;
    }

    /* Always re-check tamper + thresholds on every wake, independent of
     * what caused it -- OLED is off at this point so per spec the
     * buzzer stays silent here even on a breach. */
    if (Tamper.checkNow()) {
        EventLog.logEvent(EVT_TAMPER, Tamper.lastRawValue(), 0, 0, "poll");
    }
    checkThresholdsAndLog(r, /*oledActive=*/false);

    if (r.batteryPercent <= 10) {
        EventLog.logEvent(EVT_LOW_BATTERY, r.batteryVoltage, r.batteryPercent, 0, "");
    }
}

/* Logs + returns true if ANY threshold was breached (caller decides
 * whether to actually sound the buzzer, based on OLED state).
 *
 * Logging is EDGE-TRIGGERED (only on the not-breached -> breached
 * transition), not level-triggered, so a threshold that stays
 * breached across many consecutive polls -- e.g. the whole 5s info
 * screen window (~25 polls at 200ms) or an extended Web UI live-view
 * session (polled every 1.5s, indefinitely) -- logs ONE event, not a
 * flood of duplicates. These flags are plain (non-RTC) statics, so
 * they reset to false on every fresh deep-sleep wake, which is what
 * we want: each new wake cycle in MODE_LOGGING still gets its own
 * fresh breach check/log, exactly as before. */
/* Plain comparison against the user's saved thresholds, no logging or
 * side effects -- used by the low-power display screen (safety-status
 * text) and its continuous buzzer, independent of the edge-triggered
 * logging done in checkThresholdsAndLog() below. */
static void getThresholdBreachFlags(const SensorReadings &r, bool &luxBreached, bool &tempBreached) {
    const DeviceSettings &s = Settings.get();
    luxBreached  = r.ambientLux    >= s.threshLux;
    tempBreached = r.temperatureC  >= s.threshTempC;
}

static bool checkThresholdsAndLog(const SensorReadings &r, bool oledActive) {
    const DeviceSettings &s = Settings.get();
    static bool luxBreachActive = false;
    static bool tempBreachActive = false;
    bool breached = false;

    bool luxBreached = r.ambientLux >= s.threshLux;
    if (luxBreached) {
        if (!luxBreachActive) {
            EventLog.logEvent(EVT_LIGHT_THRESHOLD, r.ambientLux, s.threshLux, 0, "lux-breach");
        }
        breached = true;
    }
    luxBreachActive = luxBreached;

    bool tempBreached = r.temperatureC >= s.threshTempC;
    if (tempBreached) {
        if (!tempBreachActive) {
            EventLog.logEvent(EVT_TEMP_THRESHOLD, r.temperatureC, s.threshTempC, 0, "temp-breach");
        }
        breached = true;
    }
    tempBreachActive = tempBreached;

    if (breached && oledActive) {
        BuzzerDev.beepAlert();
    }
    return breached;
}

/* =====================================================================
 *  MODE_LOGGING: brief active window right after waking, to service
 *  button presses (mode switch / display) before returning to sleep.
 * ===================================================================== */
/* Requires BUTTON_CONFIRM_SAMPLES consecutive reads, spaced
 * BUTTON_CONFIRM_SAMPLE_GAP_MS apart, to all agree the pin is pressed.
 * Used right after a deep-sleep wake, where a single noisy/floating
 * read is what causes the OLED to flash on its own for no real button
 * press ("screen flickers when not touching the button"). If this
 * still flickers on real hardware, it confirms the button lines aren't
 * cleanly driven per the wiring note in config.h. */
static bool confirmButtonPressed(uint8_t pin) {
    if (!buttonIsPressed(pin)) return false;
    for (uint8_t i = 0; i < BUTTON_CONFIRM_SAMPLES; i++) {
        delay(BUTTON_CONFIRM_SAMPLE_GAP_MS);
        if (!buttonIsPressed(pin)) return false;
    }
    return true;
}

/* =====================================================================
 *  Post-boot: continuous sensor-status screen, blocks until the UI
 *  (mode) button is pressed. Only called on a true cold power-on.
 * ===================================================================== */
static void runInitialSensorStatusLoop() {
    Display.wake();
    uint32_t lastRefresh = 0;

    for (;;) {
        if (lastRefresh == 0 || millis() - lastRefresh >= SENSOR_STATUS_REFRESH_MS) {
            lastRefresh = millis();
            SensorReadings r = Sensors.readAll();
            Display.drawSensorStatusScreen(Sensors.apdsPresent(), Sensors.mpuPresent(),
                                            Sensors.bmpPresent(), r);
        }

        if (confirmButtonPressed(BTN_MODE_PIN)) {
            while (buttonIsPressed(BTN_MODE_PIN)) delay(10); /* wait release */
            BuzzerDev.beepShort();
            EventLog.logEvent(EVT_MODE_CHANGE, MODE_WEBUI, 0, 0, "button->webui(boot)");
            g_mode = MODE_WEBUI;
            return;
        }
        delay(50);
    }
}

static void runLoggingModeActiveWindow() {
    bool modePressed    = confirmButtonPressed(BTN_MODE_PIN);
    bool displayPressed = !modePressed && confirmButtonPressed(BTN_DISPLAY_PIN);

    if (modePressed) {
        /* wait for release to know short vs long press, cap wait time */
        uint32_t start = millis();
        while (buttonIsPressed(BTN_MODE_PIN) && (millis() - start) < 2000) delay(10);
        BuzzerDev.beepShort();
        EventLog.logEvent(EVT_MODE_CHANGE, MODE_WEBUI, 0, 0, "button->webui");
        g_mode = MODE_WEBUI;
        return; /* caller will start Web UI instead of sleeping */
    }

    if (displayPressed) {
        EventLog.logEvent(EVT_BUTTON_DISPLAY, 0, 0, 0, "info-screen");
        BuzzerDev.beepShort();
        Display.wake();

        uint32_t windowStart = millis();

        /* Battery/time/safety-status screen, shown for exactly
         * LOWPOWER_DISPLAY_DURATION_MS (5s). The display button is
         * only meaningful in this (low power) mode -- it's inert in
         * MODE_WEBUI. */
        while (millis() - windowStart < LOWPOWER_DISPLAY_DURATION_MS) {
            SensorReadings r = Sensors.readAll();
            bool tampered = Tamper.isLatched();
            bool luxBreached, tempBreached;
            getThresholdBreachFlags(r, luxBreached, tempBreached);

            Display.drawLowPowerInfoScreen(r, tampered, luxBreached, tempBreached);

            /* Edge-triggered CSV log entry (once per breach onset), same
             * as always. */
            checkThresholdsAndLog(r, /*oledActive=*/true);

            /* Continuous buzzer feedback for as long as a threshold or
             * tamper is breached AND this screen is up -- re-fires every
             * iteration, unlike the edge-triggered log entry above. */
            if (tampered || luxBreached || tempBreached) {
                BuzzerDev.beepAlert();
            }

            /* allow an early mode-switch (UI button) press even while
             * the info screen is up -- turns Web UI mode back on */
            if (confirmButtonPressed(BTN_MODE_PIN)) {
                while (buttonIsPressed(BTN_MODE_PIN)) delay(10); /* wait release */
                BuzzerDev.beepShort();
                EventLog.logEvent(EVT_MODE_CHANGE, MODE_WEBUI, 0, 0, "button->webui");
                g_mode = MODE_WEBUI;
                Display.sleep();
                return;
            }
            delay(LOWPOWER_DISPLAY_REFRESH_MS);
        }
        Display.sleep();
    }
}

/* =====================================================================
 *  MODE_WEBUI
 * ===================================================================== */
static void enterWebUiMode() {
    Display.wake();
    Display.drawMessage("Starting Web UI", "please wait...");
    WebUi.start();
    setStatusLed(true);   /* solid ON = "Web UI / AP active", distinct
                              from the OFF-during-sleep logging state */

    /* The URL/mode/battery/time screen stays up continuously from here
     * on -- runWebUiLoop() refreshes it every WEBUI_INFO_REFRESH_MS --
     * until Save/Start (or the mode button) drops back to low power
     * mode. No timed auto-sleep here, per spec. */
}

static void runWebUiLoop() {
    static uint32_t lastRefresh = 0;
    static uint32_t lastTamperPoll = 0;

    WebUi.handle();

    /* Mode button: leave Web UI, go back to low-power deep-sleep logging */
    if (buttonIsPressed(BTN_MODE_PIN)) {
        delay(BUTTON_DEBOUNCE_MS);
        if (buttonIsPressed(BTN_MODE_PIN)) {
            uint32_t start = millis();
            while (buttonIsPressed(BTN_MODE_PIN) && (millis() - start) < 2000) delay(10);
            exitWebUiModeBackToLogging();
            return;
        }
    }

    /* Dashboard Save/Start (settings saved) or the standalone "Go to
     * sleep" button: both request the same low-power-mode transition,
     * same effect as the physical MODE button. */
    if (WebUi.consumeSleepRequest()) {
        exitWebUiModeBackToLogging();
        return;
    }

    /* Per spec, the display button is only significant in low-power
     * (logging) mode -- intentionally not serviced here. */

    if (millis() - lastTamperPoll > TAMPER_POLL_INTERVAL_MS) {
        lastTamperPoll = millis();
        if (Tamper.checkNow()) {
            EventLog.logEvent(EVT_TAMPER, Tamper.lastRawValue(), 0, 0, "webui-poll");
        }
    }

    /* Fast-blink the status LED while tamper is latched (otherwise it
     * just stays solid ON to mean "Web UI active"). Cheap to call every
     * loop iteration -- it only touches the GPIO on its own timer. */
    statusLedServiceTamperBlink(Tamper.isLatched());

    if (millis() - lastRefresh > WEBUI_INFO_REFRESH_MS) {
        lastRefresh = millis();
        SensorReadings r = Sensors.readAll();
        uint8_t clients = WebUi.connectedClients();
        bool tampered = Tamper.isLatched();
        String url = String("\"transitlogger.local\"");

        Display.drawWebUiInfoScreen(url.c_str(), r, clients, tampered);

        /* Threshold breach + buzzer, OLED is actively showing this
         * screen throughout Web UI mode, per spec. */
        checkThresholdsAndLog(r, /*oledActive=*/true);
    }
}

static void exitWebUiModeBackToLogging() {
    BuzzerDev.beepShort();
    EventLog.logEvent(EVT_MODE_CHANGE, MODE_LOGGING, 0, 0, "button->logging");
    WebUi.stop();
    g_mode = MODE_LOGGING;
    goToDeepSleep();
}
