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

// Konfiguracja animacji płomienia
#define FLAME_ICON_WIDTH 16
#define FLAME_ICON_HEIGHT 16
#define FLAME_ANIM_INTERVAL_MS 250 // Czas między klatkami

// Bitmapy dla 3 klatek animacji płomienia (16x16 px)
const unsigned char PROGMEM flame_frame1[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x07, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x0f, 0xf0, 
    0x1f, 0xf8, 0x1f, 0xf8, 0x0f, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00
};

const unsigned char PROGMEM flame_frame2[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x07, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 
    0x0f, 0xf0, 0x1f, 0xf8, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char PROGMEM flame_frame3[] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x07, 0xc0, 0x0f, 0xe0, 0x1f, 0xf0, 0x1f, 0xf8, 
    0x1f, 0xf8, 0x0f, 0xf0, 0x07, 0xe0, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Tablica wskaźników do klatek
const unsigned char* PROGMEM const flame_frames[] = {
    flame_frame1,
    flame_frame2,
    flame_frame3
};

static Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
static unsigned long lastRefreshMs = 0;
static unsigned long lastPageSwitchMs = 0;
static unsigned long lastFlameAnimMs = 0; // Czas ostatniej klatki animacji
static int currentPage = 0;
static int currentFlameFrame = 0;    // Aktualna klatka animacji
static bool oledOk = false;

void Display::init() {
    // Adafruit_SH110X::begin(), like SSD1306's, doesn't reliably fail when
    // nothing is on the bus. I2C has a real ACK though, check that
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

// Pomocnicza funkcja do rysowania animowanego płomienia
static void drawFlameAnimation(int x, int y) {
    if (millis() - lastFlameAnimMs >= FLAME_ANIM_INTERVAL_MS) {
        lastFlameAnimMs = millis();
        currentFlameFrame = (currentFlameFrame + 1) % 3;
    }

    oled.drawBitmap(x, y, (const unsigned char*)pgm_read_ptr(&(flame_frames[currentFlameFrame])), FLAME_ICON_WIDTH, FLAME_ICON_HEIGHT, SH110X_WHITE);
}

static void drawLiveScreen() {
    // Linia 1: FireCtrl      WM- (W: WiFi, M: MQTT, S: Safety OK)
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    bool mqttOk = MQTTHandler::isConnected();
    bool safetyOk = (Safety::getState() == SafetyState::NORMAL);

    oled.setCursor(0, 0);
    oled.printf("FireCtrl v1.4  %c%c%c",
        wifiOk   ? 'W' : '-',
        mqttOk   ? 'M' : '-',
        safetyOk ? 'S' : '-');

    // Linia 2: Flue :247C +1.4
    oled.setCursor(0, 10);
    oled.printf("Flue :%.0fC %+.1f", 
        Sensors::getExhaustTemp(), 
        Sensors::getExhaustTrend());

    // Linia 3: Air  :23.6C (używamy Sensors::getInletTemp())
    oled.setCursor(0, 20);
    oled.printf("Air  :%.1fC", Sensors::getInletTemp());

    // Linia 4: Press:1007 hPa
    oled.setCursor(0, 30);
    oled.printf("Press:%.0f hPa", Sensors::getInletPressure());

    // Linia 5: Dampr: 43%
    oled.setCursor(0, 40);
    oled.printf("Dampr: %2.0f%%", Actuator::getCurrentPosition());

    // Linia 6: BURNING / Stan systemu
    oled.setCursor(0, 52);
    oled.print(Safety::stateToString(Safety::getState()));

    // ANIMACJA PŁOMIENIA w prawym dolnym rogu
    drawFlameAnimation(110, 48);
}

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