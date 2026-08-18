#include "tamper.h"
#include "config.h"
#include <Preferences.h>
#include <esp_sleep.h>

TamperSensor Tamper;

static Preferences prefs;
static const char *NVS_NS = "tamper";

/* Tamper evidence should survive a battery pull (that's a realistic
 * attack), so both the baseline and the latch live in NVS (flash),
 * not just RTC memory. */
static uint16_t loadBaseline() {
    prefs.begin(NVS_NS, true);
    uint16_t v = prefs.getUShort("baseline", 0);
    prefs.end();
    return v;
}
static void saveBaseline(uint16_t v) {
    prefs.begin(NVS_NS, false);
    prefs.putUShort("baseline", v);
    prefs.end();
}
static bool loadLatch() {
    prefs.begin(NVS_NS, true);
    bool v = prefs.getBool("latched", false);
    prefs.end();
    return v;
}
static void saveLatch(bool v) {
    prefs.begin(NVS_NS, false);
    prefs.putBool("latched", v);
    prefs.end();
}

/* Allowed drift around baseline before we call it "tamper".
 * Touch readings are noisy; tune this on the bench with your actual
 * foil geometry before deploying. */
#define TAMPER_ALLOWED_DELTA   20

void TamperSensor::begin() {
    if (loadBaseline() == 0) {
        /* first ever boot -- take an initial baseline reading */
        recalibrate();
    }
}

void TamperSensor::recalibrate() {
    /* average a few samples for a stable baseline */
    uint32_t sum = 0;
    const int N = 10;
    for (int i = 0; i < N; i++) {
        sum += touchRead(TOUCH_TAMPER_PIN);
        delay(5);
    }
    uint16_t baseline = sum / N;
    saveBaseline(baseline);
    saveLatch(false);
}

bool TamperSensor::checkNow() {
    _lastRaw = touchRead(TOUCH_TAMPER_PIN);
    uint16_t baseline = loadBaseline();
    if (baseline == 0) return false; /* not calibrated yet */

    int32_t delta = (int32_t)_lastRaw - (int32_t)baseline;
    bool tampered = abs(delta) > TAMPER_ALLOWED_DELTA;
    if (tampered) {
        saveLatch(true);
    }
    return tampered;
}

bool TamperSensor::isLatched() const {
    return loadLatch();
}

void TamperSensor::clearLatch() {
    saveLatch(false);
}

void TamperSensor::armWakeup() {
    uint16_t baseline = loadBaseline();
    if (baseline == 0) return;

    // Must be significantly lower than baseline (e.g. 50-60% of baseline or at least 35 counts)
    // to distinguish real physical touch from CPU power-rail collapse noise.
    uint16_t wakeMargin = (baseline > 60) ? (baseline / 3) : 25;
    uint16_t wakeThreshold = (baseline > wakeMargin) ? (baseline - wakeMargin) : 5;

    touchSleepWakeUpEnable(TOUCH_TAMPER_PIN, wakeThreshold);
    esp_sleep_enable_touchpad_wakeup();
}