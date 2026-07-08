#include "led_status.h"
#include "config.h"
#include "safety.h"
#include "mqtt_handler.h"
#include <FastLED.h>
#include <Arduino.h>

static CRGB led[1];
static unsigned long lastBlinkMs = 0;
static bool blinkOn = true;

void LedStatus::init() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(led, 1);
    FastLED.setBrightness(LED_BRIGHTNESS);
    led[0] = CRGB::Green;
    FastLED.show();
}

void LedStatus::update() {
    unsigned long now = millis();
    SafetyState st = Safety::getState();

    // Priority mirrors safety.cpp exactly: alarm conditions always win over
    // whatever combustion phase HA thinks it's in.
    if (st == SafetyState::CRITICAL_TEMP || st == SafetyState::HIGH_TEMP_LIMIT ||
        st == SafetyState::SENSOR_FAULT) {
        if (now - lastBlinkMs >= 400) {
            lastBlinkMs = now;
            blinkOn = !blinkOn;
        }
        led[0] = blinkOn ? CRGB::Red : CRGB::Black;
    } else if (st == SafetyState::MQTT_LOST) {
        if (now - lastBlinkMs >= 1000) {
            lastBlinkMs = now;
            blinkOn = !blinkOn;
        }
        led[0] = blinkOn ? CRGB::Purple : CRGB::Black;
    } else {
        // NORMAL - color reflects the combustion phase published by HA.
        // The ESP32 has no phase state machine of its own (by design - that
        // logic lives in Home Assistant), so this is purely a display of
        // whatever HA last told us via MQTT_TOPIC_PHASE.
        String phase = MQTTHandler::getPhase();
        if (phase == "rozpalanie") {
            led[0] = CRGB::Blue;
        } else if (phase == "wygaszanie") {
            led[0] = CRGB::Yellow;
        } else {
            led[0] = CRGB::Green;
        }
    }

    FastLED.show();
}
