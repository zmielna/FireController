#include "config.h"
#include "display.h"
#include "sensors.h"
#include "safety.h"
#include "button.h"
#include "actuator.h"
#include "mqtt_handler.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

static Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
static unsigned long lastRefreshMs = 0;
static unsigned long lastPageSwitchMs = 0;
static int currentPage = 0;
static bool oledOk = false;

void Display::init() {
    // Adafruit_SH110X::begin(), like SSD1306's, doesn't reliably fail when
    // nothing is on the bus. I2C has a real ACK though, so check that
    // directly instead of trusting the library's own return value.
    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() != 0) {
        oledOk = false;
        Serial.printf("OLED: no I2C device ACKed at 0x%02X - not connected, skipping init\n", OLED_ADDR);
        return;
    }

    oledOk = oled.begin(OLED_ADDR, true);
    if (!oledOk) {
        Serial.println("OLED: I2C device present at that address but begin() failed");
        return;
    }
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SH110X_WHITE);
    oled.display();
}

bool Display::isOk() { return oledOk; }

static void drawLiveScreen() {
    oled.setCursor(0, 0);
    oled.println("FireController v1.3");

    oled.setCursor(0, 12);
    oled.printf("Exhaust: %.0fC  d:%+.1f", Sensors::getExhaustTemp(), Sensors::getExhaustTrend());

    oled.setCursor(0, 22);
    oled.printf("Press: %.0f hPa", Sensors::getInletPressure());

    oled.setCursor(0, 32);
    oled.printf("Intake: %.0f%%%s",
        Actuator::getCurrentPosition(),
        Actuator::isSafetyOverridden() ? " (SAFE)" : "");

    oled.setCursor(0, 44);
    oled.printf("Safety: %s", Safety::stateToString(Safety::getState()));

    oled.setCursor(0, 54);
    oled.printf("MQTT: %s  BTN: %s",
        MQTTHandler::isConnected() ? "OK" : "NO",
        Button::isPressed() ? "X" : "-");
}

// Condensed version of the Serial status banner (system_status.cpp) - same
// underlying data, just laid out for a 128x64 screen instead of a terminal.
// This is deliberately a second *page*, not merged into the live screen
// above: the live screen is what you glance at during normal operation,
// this one is for "why isn't X working" without needing a laptop plugged in.
static void drawStatusScreen() {
    oled.setCursor(0, 0);
    oled.println("Sensor / Net status");

    oled.setCursor(0, 12);
    oled.printf("BMP280:  %s", Sensors::isBmpFault() ? "Missing" : "OK");

    oled.setCursor(0, 22);
    oled.printf("MAX6675: %s", Sensors::isMaxFault() ? "Missing" : "OK");

    oled.setCursor(0, 32);
    if (WiFi.status() == WL_CONNECTED) {
        oled.printf("WiFi:    OK %ddBm", WiFi.RSSI());
    } else {
        oled.print("WiFi:    Connecting");
    }

    oled.setCursor(0, 44);
    oled.printf("MQTT:    %s", MQTTHandler::isConnected() ? "OK" : "Waiting");

    oled.setCursor(0, 54);
    oled.printf("Heap: %u kB", (unsigned)(ESP.getFreeHeap() / 1024));
}

// Same chip/board info as the Serial status banner's header
// (system_status.cpp) - CPU model, clock, flash size, firmware version.
// Doesn't duplicate MAC/WiFi details, those already live on the status page.
static void drawChipScreen() {
    oled.setCursor(0, 0);
    oled.println("Board info");

    oled.setCursor(0, 12);
    oled.printf("FW: %s", FIRMWARE_VERSION);

    oled.setCursor(0, 22);
    oled.printf("CPU: %s", ESP.getChipModel());

    oled.setCursor(0, 32);
    oled.printf("Clock: %u MHz", (unsigned)ESP.getCpuFreqMHz());

    oled.setCursor(0, 44);
    oled.printf("Flash: %u MB", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));

    oled.setCursor(0, 54);
    oled.printf("Cores: %d", ESP.getChipCores());
}

void Display::update() {
    if (!oledOk) {
        return;
    }
    unsigned long now = millis();

    if (now - lastPageSwitchMs >= DISPLAY_PAGE_ROTATE_MS) {
        lastPageSwitchMs = now;
        currentPage = (currentPage + 1) % 3;
    }

    if (now - lastRefreshMs < DISPLAY_REFRESH_MS) {
        return;
    }
    lastRefreshMs = now;

    oled.clearDisplay();
    switch (currentPage) {
        case 0: drawLiveScreen(); break;
        case 1: drawStatusScreen(); break;
        case 2: drawChipScreen(); break;
    }
    oled.display();
}
