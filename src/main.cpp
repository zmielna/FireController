#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "safety.h"
#include "display.h"
#include "mqtt_handler.h"
#include "button.h"

void setup() {
    Serial.begin(115200);

    Sensors::init();
    Safety::init();
    Display::init();
    MQTTHandler::init();
    Button::init();
}

void loop() {
    Sensors::update();
    Safety::update();
    Display::update();
    MQTTHandler::update();
    Button::update();
}
