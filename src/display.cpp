#include "config.h"
#include "display.h"
#include "sensors.h"
#include "safety.h"
#include "button.h"
#include "actuator.h"
#include "mqtt_handler.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

static Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
static unsigned long lastRefreshMs = 0;
static bool oledOk = false;

void Display::init() {
    // Adafruit_SSD1306::begin() doesn't reliably fail when nothing is on the
    // bus - some versions return true regardless (same class of problem as
    // MAX31856's begin(), just for a different chip). Unlike SPI though,
    // I2C actually has a real ACK we can check directly, so use that instead
    // of trusting the library's own return value.
    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() != 0) {
        oledOk = false;
        Serial.printf("OLED: no I2C device ACKed at 0x%02X - not connected, skipping init\n", OLED_ADDR);
        return;
    }

    oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (!oledOk) {
        Serial.println("OLED: I2C device present at that address but begin() failed");
        return;
    }
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
}

bool Display::isOk() { return oledOk; }

void Display::update() {
    if (!oledOk) {
        return;
    }
    unsigned long now = millis();
    if (now - lastRefreshMs < DISPLAY_REFRESH_MS) {
        return;
    }
    lastRefreshMs = now;

    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("FireController v1.2");

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

    oled.display();
}
