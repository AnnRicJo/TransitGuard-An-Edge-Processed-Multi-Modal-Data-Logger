#include "webui.h"
#include "config.h"
#include "settings.h"
#include "sensors.h"
#include "event_log.h"
#include "tamper.h"
#include "rtc_time.h"
#include "buzzer.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

WebUiServer WebUi;
static WebServer server(WEBSERVER_PORT);

/* Set by handleSleep(), consumed once by main.cpp's runWebUiLoop() --
 * see consumeSleepRequest(). Plain static bool: single-core Arduino
 * loop, no concurrent access. */
static volatile bool s_sleepRequested = false;

/* ---------------------------------------------------------------------
 *  Captive-portal DNS: answers EVERY hostname lookup with our own AP
 *  IP, so the phone/laptop either auto-pops the "sign in to network"
 *  page, or the user can just type anything into the address bar.
 * ------------------------------------------------------------------- */
static DNSServer dnsServer;
static const byte DNS_PORT = 53;

/* Friendly name for mDNS: http://transitlogger.local (no IP needed,
 * but unlike the DNS trick above this only works for THIS exact name,
 * and needs client-side mDNS/Bonjour support -- most phones have it,
 * some older Windows machines don't without installing Bonjour). */
static const char *MDNS_HOSTNAME = "transitlogger";

/* -------------------------------------------------------------------
 *  Small helper: consistent page chrome (kept in PROGMEM to save RAM)
 * ------------------------------------------------------------------- */
static const char PAGE_HEAD[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Transit Logger</title>"
    "<style>"
    "body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:16px;}"
    "h1{font-size:1.3em;} .card{background:#1c1c1c;border-radius:10px;padding:14px;margin-bottom:14px;}"
    "label{display:block;margin-top:8px;font-size:.85em;color:#aaa;}"
    "input{width:100%;box-sizing:border-box;padding:8px;margin-top:4px;border-radius:6px;border:1px solid #444;background:#000;color:#eee;}"
    "button,a.btn{display:inline-block;margin-top:12px;padding:10px 16px;border:none;border-radius:6px;background:#3a7bd5;color:#fff;text-decoration:none;font-size:1em;cursor:pointer;}"
    "button.warn{background:#c0392b;} .row{display:flex;gap:10px;flex-wrap:wrap;}"
    ".val{font-size:1.4em;font-weight:bold;} .tamper{background:#c0392b;padding:10px;border-radius:8px;margin-bottom:14px;}"
    "</style></head><body>";

static const char PAGE_FOOT[] PROGMEM = "</body></html>";

/* ===================================================================== */
void WebUiServer::start() {
    const DeviceSettings &s = Settings.get();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(s.apSsid, s.apPass);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatusJson);
    server.on("/settings", HTTP_GET, handleSettingsGet);
    server.on("/settings", HTTP_POST, handleSettingsPost);
    server.on("/download", HTTP_GET, handleDownload);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/synctime", HTTP_POST, handleSyncTime);
    server.on("/tamper/clear", HTTP_POST, handleTamperClear);
    server.on("/sleep", HTTP_POST, handleSleep);
    server.onNotFound(handleNotFound);

    server.begin();
    startDns();
    startMdns();
    _running = true;

    Serial.print(F("[WebUi] AP started, SSID="));
    Serial.print(s.apSsid);
    Serial.print(F(" IP="));
    Serial.println(WiFi.softAPIP());
    Serial.println(F("[WebUi] Connect, then just open any URL -- the "
                      "captive-portal DNS redirects everything here. "
                      "Or use http://transitlogger.local"));
}

void WebUiServer::stop() {
    stopMdns();
    stopDns();
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
}

void WebUiServer::handle() {
    if (!_running) return;
    dnsServer.processNextRequest();   /* must be pumped every loop, like server.handleClient() */
    server.handleClient();
}

void WebUiServer::startDns() {
    /* "*" = wildcard, answer for ANY hostname the client asks about */
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

void WebUiServer::stopDns() {
    dnsServer.stop();
}

void WebUiServer::startMdns() {
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WEBSERVER_PORT);
    } else {
        Serial.println(F("[WebUi] mDNS responder failed to start"));
    }
}

void WebUiServer::stopMdns() {
    MDNS.end();
}

uint8_t WebUiServer::connectedClients() const {
    return WiFi.softAPgetStationNum();
}

/* Set by handleSleep(), consumed once by main.cpp's runWebUiLoop() --
 * see consumeSleepRequest(). Plain static bool: single-core Arduino
 * loop, no concurrent access. */
static uint32_t s_sleepRequestTimestamp = 0;   // <-- Add this line here

bool WebUiServer::consumeSleepRequest() {
    // Wait ~200ms after response is sent so the TCP stack can complete packet delivery
    if (!s_sleepRequested || (millis() - s_sleepRequestTimestamp < 200)) {
        return false;
    }
    s_sleepRequested = false;
    return true;
}

/* ===================================================================== */
void WebUiServer::handleRoot() {
    bool tampered = Tamper.isLatched();
    const DeviceSettings &s = Settings.get();

    String html;
    html.reserve(4096);
    html += FPSTR(PAGE_HEAD);
    html += F("<h1>Transit Package Logger</h1>");

    if (tampered) {
        html += F("<div class='tamper'><b>TAMPER DETECTED</b><br>"
                   "The tamper-evidence foil reported an out-of-range reading."
                   "<form method='POST' action='/tamper/clear'>"
                   "<button class='warn' type='submit'>Acknowledge &amp; clear</button></form></div>");
    }

    html += F("<div class='card'><h3>Live status</h3>"
               "<div class='row'>"
               "<div><label>Ambient light</label><div class='val' id='lux'>--</div></div>"
               "<div><label>Battery</label><div class='val' id='batt'>--</div></div>"
               "<div><label>Temp</label><div class='val' id='temp'>--</div></div>"
               "<div><label>Pressure</label><div class='val' id='pressure'>--</div></div>"
               "<div><label>Shock</label><div class='val' id='accel'>--</div></div>"
               "</div>"
               "<label>Device time</label><div id='time'>--</div>"
               "<label>Events logged this transit</label><div id='count'>--</div>"
               "</div>");

    html += F("<div class='card'><h3>Event log</h3>"
               "<a class='btn' href='/download'>Download CSV</a> "
               "<form style='display:inline' method='POST' action='/reset' "
               "onsubmit=\"return confirm('This clears the current transit log. Continue?')\">"
               "<button class='warn' type='submit'>Reset for next transit</button></form></div>");

    html += F("<div class='card'><h3>Power</h3>"
               "<p style='color:#aaa;font-size:.85em'>Ends the Wi-Fi dashboard and drops the "
               "device back into low-power deep-sleep logging -- same as pressing the physical "
               "MODE button on the unit.</p>"
               "<form method='POST' action='/sleep' "
               "onsubmit=\"return confirm('Return to low-power logging mode now? This ends the Wi-Fi session.')\">"
               "<button class='warn' type='submit'>Go to sleep now</button></form></div>");
    html += F("<div class='card'><h3>Alert thresholds</h3>"
               "<form method='POST' action='/settings'>"
               "<label>Ambient light threshold (lux)</label>"
               "<input type='number' step='1' name='lux' value='");
    html += String(s.threshLux, 0);
    html += F("'>"
               "<label>Temperature threshold (°C)</label>"
               "<input type='number' step='0.1' name='temp' value='");
    html += String(s.threshTempC, 1);
    html += F("'>"
               "<label>Motion/shock sensitivity (1=very sensitive, 255=insensitive)</label>"
               "<input type='number' step='1' min='1' max='255' name='motion' value='");
    html += String(s.motionThreshold);  
    html += F("'>"
               "<label>Logging interval -- seconds between recorded readings</label>"
               "<input type='number' step='1' min='");
    html += String((unsigned long)MIN_LOGGING_INTERVAL_SEC);
    html += F("' max='");
    html += String((unsigned long)MAX_LOGGING_INTERVAL_SEC);
    html += F("' name='logintvl' value='");
    html += String((unsigned long)s.loggingIntervalSec);
    html += F("'>"
               "<p style='color:#aaa;font-size:.85em'>Data is recorded at this interval, "
               "and also immediately whenever a threshold above is breached "
               "(motion/proximity interrupt, or a light/temperature threshold crossing).</p>"
               "<p style='color:#aaa;font-size:.85em'>Saving starts logging: the device "
               "drops the Wi-Fi dashboard and returns to low-power mode "
               "immediately after you hit Save.</p>"
               "<button type='submit'>Save &amp; Start logging</button>"
               "</form>"
               "<button type='button' class='btn' onclick=\""
               "const d=new Date();"
               "const epoch=Math.floor(d.getTime()/1000);"
               "const tzOffsetSec=-d.getTimezoneOffset()*60;"
               "fetch('/synctime',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'epoch='+epoch+'&tz='+tzOffsetSec})"
               ".then(r=>r.text()).then(msg=>{alert(msg);poll();});"
               "\" style='margin-top:6px'>"
               "Sync time with this device</button>"
               "</div>");
    html += F("<script>"
              "async function poll(){"
              "try{const r=await fetch('/status');const j=await r.json();"
              "lux.textContent=j.lux+' lx';"
              "batt.textContent=j.battPct+'% ('+j.battV+'V)';"
              "temp.textContent=j.tempC+'C';"
              "pressure.textContent=j.pressureHPa+' hPa';"
              "accel.textContent=j.accelG+' g';"
              "time.textContent=j.time;"
              "count.textContent=j.events;"
              "}catch(e){}}"
              "poll();setInterval(poll,3000);"
              "</script>");
    html += FPSTR(PAGE_FOOT);

    server.send(200, "text/html", html);
}

void WebUiServer::handleStatusJson() {
    SensorReadings r = Sensors.readAll();
    String json = "{";
    json += "\"lux\":" + String(r.ambientLux) + ",";
    json += "\"battPct\":" + String(r.batteryPercent) + ",";
    json += "\"battV\":" + String(r.batteryVoltage, 2) + ",";
    json += "\"tempC\":" + String(r.temperatureC, 1) + ",";
    json += "\"pressureHPa\":" + String(r.pressureHPa, 0) + ",";
    json += "\"altitudeM\":" + String(r.altitudeM, 1) + ",";
    json += "\"accelG\":" + String(r.accelMagnitude_g, 2) + ",";
    json += "\"time\":\"" + RtcTime.nowFormatted() + "\",";
    json += "\"events\":" + String((unsigned)EventLog.entryCount()) + ",";
    json += "\"tamper\":" + String(Tamper.isLatched() ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

void WebUiServer::handleSettingsGet() {
    /* Settings are shown inline on the root page; redirect there. */
    server.sendHeader("Location", "/");
    server.send(303);
}

void WebUiServer::handleSettingsPost() {
    
    float lux  = server.arg("lux").toFloat();
    float temp = server.arg("temp").toFloat();
    float prox = server.hasArg("prox") ? server.arg("prox").toFloat() : 0.0f;
    Settings.setThresholds(lux, prox, temp);

    uint8_t motion = (uint8_t)constrain(server.arg("motion").toInt(), 1, 255);
    Settings.setMotionThreshold(motion);

    uint32_t logIntvl = (uint32_t)constrain((long)server.arg("logintvl").toInt(),
                                             (long)MIN_LOGGING_INTERVAL_SEC,
                                             (long)MAX_LOGGING_INTERVAL_SEC);
    Settings.setLoggingInterval(logIntvl);

    String hssid = server.arg("hssid");
    String hpass = server.arg("hpass");
    Settings.setHomeWifi(hssid.c_str(), hpass.c_str());

    BuzzerDev.beepShort();

    server.sendHeader("Location", "/");
    server.send(303);
    s_sleepRequested = true;
    s_sleepRequestTimestamp = millis();
}

void WebUiServer::handleDownload() {
    if (!LittleFS.exists(LOG_FILE_PATH)) {
        server.send(404, "text/plain", "No log file yet");
        return;
    }
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    if (!f) {
        server.send(500, "text/plain", "Failed to open log file");
        return;
    }
    server.sendHeader("Content-Disposition", "attachment; filename=transit_log.csv");
    server.streamFile(f, "text/csv");
    f.close();
}

void WebUiServer::handleReset() {
    EventLog.resetLog();
    BuzzerDev.beepShort();
    server.sendHeader("Location", "/");
    server.send(303);
}

void WebUiServer::handleSyncTime() {
    if (server.hasArg("epoch")) {
        time_t epoch = (time_t)strtoul(server.arg("epoch").c_str(), NULL, 10);
        int32_t tzSec = server.hasArg("tz") ? server.arg("tz").toInt() : 0;
        
        if (epoch > 1000000000UL) {
            RtcTime.setEpochTime(epoch, tzSec);
            BuzzerDev.beepShort();
            server.send(200, "text/plain", "Time synced successfully!");
            return;
        }
    }
    server.send(400, "text/plain", "Invalid time data");
}

void WebUiServer::handleTamperClear() {
    Tamper.clearLatch();
    BuzzerDev.beepShort();
    server.sendHeader("Location", "/");
    server.send(303);
}


void WebUiServer::handleSleep() {
    // In handleSleep():
    server.send(200, "text/plain", "Going back to sleep / logging mode.");
    s_sleepRequested = true;
    s_sleepRequestTimestamp = millis();
}

void WebUiServer::handleNotFound() {
    /* This is the other half of the captive-portal trick: the DNS
     * server already pointed the browser at our IP for whatever
     * hostname it asked for (e.g. the OS's own connectivity-check
     * URL, like Apple's captive.apple.com or Android's
     * connectivitycheck.gstatic.com) -- redirecting every unknown
     * path back to "/" is what makes the OS realize this is a
     * captive portal and pop the sign-in browser automatically. */
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
}
