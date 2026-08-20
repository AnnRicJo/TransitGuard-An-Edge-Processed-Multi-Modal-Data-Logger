#pragma once
/* =====================================================================
 *  webui.h
 *  --------------------------------------------------------------
 *  Runs the ESP32 as a Wi-Fi Access Point + HTTP server for the
 *  "Web UI mode": dashboard, threshold settings, CSV log download,
 *  and "reset for next transit". Also exposes helpers the main loop
 *  uses to know if it's safe/time to drop back to logging mode.
 * =====================================================================
 */
#include <Arduino.h>

class WebUiServer {
public:
    void start();     /* brings up the AP + HTTP server                 */
    void stop();      /* tears both down cleanly before returning to
                          deep-sleep logging mode                        */
    void handle();    /* call every loop() iteration while in Web UI mode */

    bool isRunning() const { return _running; }
    uint8_t connectedClients() const;

    /* True (and clears itself) exactly once after the dashboard's
     * "Go to sleep" button has posted to /sleep -- lets main.cpp treat
     * a web-triggered request to return to low-power logging mode the
     * same as a physical MODE button press. */
    bool consumeSleepRequest();

private:
    bool _running = false;
    void startDns();
    void stopDns();
    void startMdns();
    void stopMdns();
    static void handleRoot();
    static void handleStatusJson();
    static void handleSettingsGet();
    static void handleSettingsPost();
    static void handleDownload();
    static void handleReset();
    static void handleSyncTime();
    static void handleTamperClear();
    static void handleSleep();
    static void handleNotFound();
    static void handleCalibrate();  
};

extern WebUiServer WebUi;
