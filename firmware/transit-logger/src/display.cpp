#include "display.h"
#include "config.h"
#include "rtc_time.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

DisplayManager Display;

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1 /* no reset pin */);

bool DisplayManager::begin() {
    _present = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
    if (_present) {
        oled.setTextColor(SSD1306_WHITE);
        oled.cp437(true);
    }
    return _present;
}

void DisplayManager::sleep() {
    if (_present) {
        oled.clearDisplay();
        oled.display();
        oled.ssd1306_command(SSD1306_DISPLAYOFF);
        delay(10);
    }
}
void DisplayManager::wake() {
    if (_present) oled.ssd1306_command(SSD1306_DISPLAYON);
}

void DisplayManager::drawTamperBanner() {
    /* inverted-video strip along the very top of the screen -- hard to miss */
    oled.fillRect(0, 0, OLED_WIDTH, 10, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(2, 1);
    oled.print(F("!! TAMPER DETECTED !!"));
    oled.setTextColor(SSD1306_WHITE); /* restore for rest of screen */
}

void DisplayManager::drawWifiGlyph(int x, int y, bool active) {
    /* simple 3-bar "signal" glyph; hollow bars if AP/web not reachable */
    for (int bar = 0; bar < 3; bar++) {
        int h = 3 + bar * 3;
        int bx = x + bar * 5;
        int by = y + (9 - h);
        if (active) oled.fillRect(bx, by, 3, h, SSD1306_WHITE);
        else        oled.drawRect(bx, by, 3, h, SSD1306_WHITE);
    }
}

void DisplayManager::drawBootSplash() {
    if (!_present) return;
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(4, 10);
    oled.print(F("TRANSIT"));
    oled.setCursor(4, 32);
    oled.print(F("LOGGER"));
    oled.setTextSize(1);
    oled.setCursor(4, 54);
    oled.print(F("booting..."));
    oled.display();
}

void DisplayManager::drawMessage(const char *line1, const char *line2) {
    if (!_present) return;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 20);
    oled.println(line1);
    oled.println(line2);
    oled.display();
}

void DisplayManager::drawSensorStatusScreen(bool apdsPresent, bool mpuPresent, bool bmpPresent,
                                             const SensorReadings &r) {
    if (!_present) return;
    oled.clearDisplay();
    oled.setTextSize(1);

    oled.setCursor(0, 0);
    oled.println(F("== SENSOR STATUS =="));

    /* One line per sensor: name + recognised/active state. A sensor
     * that didn't answer on the I2C bus at begin() reads "NOT FOUND";
     * one that did is treated as active for the whole session (there's
     * no separate ongoing health check beyond the initial probe). */
    oled.setCursor(0, 14);
    oled.print(F("MPU6050  : "));
    oled.println(mpuPresent ? F("OK/ACTIVE") : F("NOT FOUND"));

    oled.setCursor(0, 24);
    oled.print(F("BMP180   : "));
    oled.println(bmpPresent ? F("OK/ACTIVE") : F("NOT FOUND"));

    oled.setCursor(0, 34);
    oled.print(F("APDS9960 : "));
    oled.println(apdsPresent ? F("OK/ACTIVE") : F("NOT FOUND"));

    oled.setCursor(0, 46);
    oled.print(F("Battery: "));
    oled.print(r.batteryPercent);
    oled.print(F("% "));
    oled.print(r.batteryVoltage, 2);
    oled.println(F("V"));

    oled.setCursor(0, 56);
    oled.print(F("Press UI btn for Web UI"));

    oled.display();
}

void DisplayManager::drawWebUiInfoScreen(const char *url, const SensorReadings &r,
                                          uint8_t clientsConnected, bool tampered) {
    if (!_present) return;
    (void)clientsConnected; /* not shown -- keeps this screen to the 4
                                items the spec asks for, and fits every
                                line on-panel even with the tamper
                                banner's extra 12px eating into yStart */
    oled.clearDisplay();

    int yStart = 0;
    if (tampered) {
        drawTamperBanner();
        yStart = 12;
    }

    /* Wi-Fi / web-access glyph, top-right corner */
    drawWifiGlyph(OLED_WIDTH - 18, yStart, true);

    oled.setTextSize(1);
    oled.setCursor(0, yStart);
    oled.println(F("== WEB UI MODE =="));

    oled.setCursor(0, yStart + 10);
    oled.println(F("Open in browser:"));
    oled.setCursor(0, yStart + 20);
    oled.println(url);

    oled.setCursor(0, yStart + 32);
    oled.print(F("Batt "));
    oled.print(r.batteryPercent);
    oled.print(F("% "));
    oled.print(r.batteryVoltage, 2);
    oled.println(F("V"));

    oled.setCursor(0, yStart + 42);
    //oled.print(F("Time: "));
    oled.println(RtcTime.nowFormatted());

    oled.display();
}

void DisplayManager::drawLowPowerInfoScreen(const SensorReadings &r, bool tampered,
                                             bool luxBreached, bool tempBreached) {
    if (!_present) return;
    oled.clearDisplay();

    int yStart = 0;
    if (tampered) {
        drawTamperBanner();
        yStart = 12;
    }

    oled.setTextSize(1);
    oled.setCursor(0, yStart);
    oled.println(F("== PACKAGE STATUS =="));

    oled.setCursor(0, yStart + 14);
    oled.print(F("Batt "));
    oled.print(r.batteryPercent);
    oled.print(F("% "));
    oled.print(r.batteryVoltage, 2);
    oled.println(F("V"));

    oled.setCursor(0, yStart + 26);
    //oled.print(F("Time: "));
    oled.println(RtcTime.nowFormatted());

    bool breached = tampered || luxBreached || tempBreached;
    oled.setCursor(0, yStart + 38);
    oled.print(F("Safety: "));
    oled.println(breached ? F("*** ALERT ***") : F("OK"));

    /* Only printed when something's actually wrong, so the screen
     * doesn't waste its last line when everything's fine. */
    if (breached) {
        oled.setCursor(0, yStart + 48);
        if (tampered)      oled.print(F("TAMPER "));
        if (luxBreached)   oled.print(F("LIGHT "));
        if (tempBreached)  oled.print(F("TEMP "));
    }

    oled.display();
}
