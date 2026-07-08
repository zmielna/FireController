#include "button.h"
#include "config.h"
#include <Arduino.h>

static bool pressed = false;
static bool pressedEdge = false;
static bool lastReading = true;
static unsigned long lastChangeMs = 0;

void Button::init() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void Button::update() {
    pressedEdge = false;

    bool reading = digitalRead(BUTTON_PIN) == LOW;
    unsigned long now = millis();

    if (reading != lastReading) {
        lastChangeMs = now;
        lastReading = reading;
    }

    if (now - lastChangeMs >= BUTTON_DEBOUNCE_MS) {
        if (reading && !pressed) {
            pressedEdge = true;
        }
        pressed = reading;
    }
}

bool Button::isPressed() { return pressed; }
bool Button::wasPressed() { return pressedEdge; }
