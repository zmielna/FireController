#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "safety.h"
#include "display.h"
#include "mqtt_handler.h"
#include "button.h"
#include "actuator.h"
#include "led_status.h"

// Manual presets cycled by the button. There is currently no AUTO/MANUAL
// lock: whichever sets the desired position last (button or MQTT) wins.
// Pressing the button then having HA immediately push a new MQTT position
// will silently overwrite your manual choice. Fine for now, but worth
// adding an explicit mode flag before relying on this day-to-day.
static const float MANUAL_PRESETS[] = {25.0f, 50.0f, 75.0f, 100.0f};
static int manualPresetIdx = 3; // start fully open, matches fail-open default

void setup() {
    Serial.begin(115200);

    Sensors::init();
    Safety::init();
    Actuator::init();
    Display::init();
    Button::init();
    LedStatus::init();
    MQTTHandler::init();
}

void loop() {
    Sensors::update();
    Safety::update();
    Button::update();

    if (Button::wasPressed()) {
        manualPresetIdx = (manualPresetIdx + 1) % 4;
        Actuator::setDesiredPosition(MANUAL_PRESETS[manualPresetIdx]);
    }

    Actuator::update();
    MQTTHandler::update();
    LedStatus::update();
    Display::update();
}
