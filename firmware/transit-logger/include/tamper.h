#pragma once
/* =====================================================================
 *  tamper.h
 *  --------------------------------------------------------------
 *  Capacitive-touch tamper sense for the aluminium foil lining the
 *  inner box edges. Works both as an intrusion detector (someone
 *  touching/bridging the foil) and a cut-foil detector (capacitance
 *  drops sharply if the foil trace is severed) by flagging ANY large
 *  deviation from a calibrated baseline, in either direction.
 *
 *  The tamper flag is LATCHED (stays set) in RTC memory across deep
 *  sleep, and is only cleared explicitly from the Web UI, so a brief
 *  tamper event during logging mode won't be silently missed.
 * =====================================================================
 */
#include <Arduino.h>

class TamperSensor {
public:
    void begin();                 /* takes a calibration baseline reading */
    void recalibrate();           /* call after sealing the box, from Web UI */

    bool checkNow();              /* returns true if THIS reading is out of range */
    bool isLatched() const;       /* true if tamper has EVER fired since last clear */
    void clearLatch();

    uint16_t lastRawValue() const { return _lastRaw; }

    /* Arms the ESP32 touch-pad wakeup source for deep sleep so tampering
     * itself can wake the device even with no button/sensor interrupt.
     * Computes the wake threshold internally from the stored baseline. */
    void armWakeup();

private:
    uint16_t _lastRaw = 0;
};

extern TamperSensor Tamper;
