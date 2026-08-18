#include "buzzer.h"
#include "config.h"

Buzzer BuzzerDev;

bool Buzzer::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    return true; /* direct GPIO, nothing to probe/fail like an I2C device */
}

void Buzzer::tone(uint32_t freqHz, uint32_t durationMs) {
    if (freqHz == 0) return;
    uint32_t periodUs     = 1000000UL / freqHz;
    uint32_t halfPeriodUs = periodUs / 2;
    uint32_t cycles       = (durationMs * 1000UL) / periodUs;

    for (uint32_t i = 0; i < cycles; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delayMicroseconds(halfPeriodUs);
        digitalWrite(BUZZER_PIN, LOW);
        delayMicroseconds(halfPeriodUs);
    }
    digitalWrite(BUZZER_PIN, LOW);
}

void Buzzer::beepShort() {
    tone(BUZZER_TONE_HZ, BEEP_SHORT_MS);
}

void Buzzer::beepAlert() {
    for (uint8_t i = 0; i < BEEP_ALERT_REPEATS; i++) {
        tone(BUZZER_TONE_HZ, BEEP_ALERT_ON_MS);
        delay(BEEP_ALERT_OFF_MS);
    }
}
