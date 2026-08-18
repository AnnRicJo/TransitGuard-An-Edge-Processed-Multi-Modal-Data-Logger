#pragma once
/* =====================================================================
 *  buzzer.h
 *  --------------------------------------------------------------
 *  Plain passive piezo buzzer wired straight to BUZZER_PIN (active
 *  high, no driver module). A passive buzzer needs an actual
 *  oscillating square wave to produce sound -- holding the pin
 *  constantly HIGH just gives you silence and a hot GPIO. This driver
 *  bit-bangs that tone directly, so there's no ledc/tone() API version
 *  mismatch to worry about between ESP32 Arduino core releases.
 * =====================================================================
 */
#include <Arduino.h>

class Buzzer {
public:
    bool begin();          /* just configures the GPIO as output */

    void beepShort();      /* single short click -- button feedback */
    void beepAlert();      /* repeated beeps -- threshold breach warning
                               (caller decides WHEN to call this, per the
                               "only warn while OLED is active" rule) */

private:
    void tone(uint32_t freqHz, uint32_t durationMs);
};

extern Buzzer BuzzerDev;
