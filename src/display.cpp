#include "display.h"
#include "sensors.h"
#include "safety.h"
#include "button.h"
#include "mqtt_handler.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

static Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

void Display::init() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED init failed");
        return;
    }
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
}

void Display::update() {
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("FireController v1.0");

    oled.setCursor(0, 16);
    oled.printf("Temp: %.1f C", Sensors::getExhaustTemp());

    oled.setCursor(0, 26);
    oled.printf("Press: %.0f hPa", Sensors::getInletPressure());

    oled.setCursor(0, 40);
    if (Safety::isOverheat()) {
        oled.println("Safety: OVERHEAT!");
    } else if (Safety::isSensorFault()) {
        oled.println("Safety: SENSOR ERR");
    } else {
        oled.println("Safety: OK");
    }

    oled.setCursor(0, 52);
    oled.printf("MQTT: %s   BTN: %s",
        MQTTHandler::isConnected() ? "OK" : "NO",
        Button::isPressed() ? "X" : "-");

    oled.display();
}
