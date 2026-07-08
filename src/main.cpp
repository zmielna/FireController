#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "safety.h"
#include "display.h"
#include "mqtt_handler.h"
#include "button.h"
#include "actuator.h"

void setup() {
    Serial.begin(115200);

    Sensors::init();
    Safety::init();
    Actuator::init();
    Display::init();
    Button::init();
    MQTTHandler::init();
}

void loop() {
    Sensors::update();
    Safety::update();
    Button::update();

    if (Button::wasPressed()) {
        Actuator::toggle();
    }

    Actuator::update();
    MQTTHandler::update();
    Display::update();
}
