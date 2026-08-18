#include "config.h"

const char *eventTypeToStr(uint8_t t) {
    switch (t) {
        case EVT_BOOT:            return "BOOT";
        case EVT_PERIODIC:        return "PERIODIC";
        case EVT_PROXIMITY:       return "PROXIMITY";
        case EVT_LIGHT_THRESHOLD: return "LIGHT_THRESHOLD";
        case EVT_GESTURE:         return "GESTURE";
        case EVT_TEMP_THRESHOLD:  return "TEMP_THRESHOLD";
        case EVT_TAMPER:          return "TAMPER";
        case EVT_MODE_CHANGE:     return "MODE_CHANGE";
        case EVT_BUTTON_DISPLAY:  return "BUTTON_DISPLAY";
        case EVT_LOG_RESET:       return "LOG_RESET";
        case EVT_LOW_BATTERY:     return "LOW_BATTERY";
        case EVT_MOTION:          return "MOTION";
        default:                  return "UNKNOWN";
    }
}
