#include "settings.h"
#include "config.h"
#include <Preferences.h>

SettingsManager Settings;   /* single global instance used across the app */

static Preferences prefs;
static const char *NVS_NAMESPACE = "translogger";

void SettingsManager::load() {
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);

    _settings.threshLux      = prefs.getFloat("lux",  DEFAULT_THRESH_LUX);
    _settings.threshProximity= prefs.getFloat("prox", DEFAULT_THRESH_PROX);
    _settings.threshTempC    = prefs.getFloat("temp", DEFAULT_THRESH_TEMP_C);
    _settings.motionThreshold= prefs.getUChar("motion", MPU_MOTION_THRESHOLD_DEFAULT);
    _settings.loggingIntervalSec = prefs.getULong("logIntvl", DEFAULT_LOGGING_INTERVAL_SEC);

    prefs.getString("hSsid", _settings.homeWifiSsid, sizeof(_settings.homeWifiSsid));
    prefs.getString("hPass", _settings.homeWifiPass, sizeof(_settings.homeWifiPass));

    /* Default AP SSID includes the last 3 bytes of the MAC for uniqueness */
    String defaultApSsid = String(DEFAULT_AP_SSID_PREFIX) + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    String storedApSsid = prefs.getString("aSsid", defaultApSsid);
    String storedApPass = prefs.getString("aPass", DEFAULT_AP_PASSWORD);
    storedApSsid.toCharArray(_settings.apSsid, sizeof(_settings.apSsid));
    storedApPass.toCharArray(_settings.apPass, sizeof(_settings.apPass));

    prefs.end();
}

void SettingsManager::save() {
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    prefs.putFloat("lux",  _settings.threshLux);
    prefs.putFloat("prox", _settings.threshProximity);
    prefs.putFloat("temp", _settings.threshTempC);
    prefs.putUChar("motion", _settings.motionThreshold);
    prefs.putULong("logIntvl", _settings.loggingIntervalSec);
    prefs.putString("hSsid", _settings.homeWifiSsid);
    prefs.putString("hPass", _settings.homeWifiPass);
    prefs.putString("aSsid", _settings.apSsid);
    prefs.putString("aPass", _settings.apPass);
    prefs.end();
}

void SettingsManager::begin() {
    load();
}

void SettingsManager::setThresholds(float lux, float prox, float tempC) {
    _settings.threshLux = lux;
    _settings.threshProximity = prox;
    _settings.threshTempC = tempC;
    save();
}

void SettingsManager::setMotionThreshold(uint8_t thr) {
    _settings.motionThreshold = thr;
    save();
}

void SettingsManager::setLoggingInterval(uint32_t sec) {
    if (sec < MIN_LOGGING_INTERVAL_SEC) sec = MIN_LOGGING_INTERVAL_SEC;
    if (sec > MAX_LOGGING_INTERVAL_SEC) sec = MAX_LOGGING_INTERVAL_SEC;
    _settings.loggingIntervalSec = sec;
    save();
}

void SettingsManager::setHomeWifi(const char *ssid, const char *pass) {
    strncpy(_settings.homeWifiSsid, ssid, sizeof(_settings.homeWifiSsid) - 1);
    _settings.homeWifiSsid[sizeof(_settings.homeWifiSsid) - 1] = '\0';
    strncpy(_settings.homeWifiPass, pass, sizeof(_settings.homeWifiPass) - 1);
    _settings.homeWifiPass[sizeof(_settings.homeWifiPass) - 1] = '\0';
    save();
}

void SettingsManager::setApCredentials(const char *ssid, const char *pass) {
    strncpy(_settings.apSsid, ssid, sizeof(_settings.apSsid) - 1);
    _settings.apSsid[sizeof(_settings.apSsid) - 1] = '\0';
    strncpy(_settings.apPass, pass, sizeof(_settings.apPass) - 1);
    _settings.apPass[sizeof(_settings.apPass) - 1] = '\0';
    save();
}

void SettingsManager::resetToDefaults() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    load();
}
