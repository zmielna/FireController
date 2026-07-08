#include "button.h"
#include <Arduino.h>
#include "config.h"

static bool pressed = false;

void Button::init() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void Button::update() {
    pressed = digitalRead(BUTTON_PIN) == LOW;
}

bool Button::isPressed() {
    return pressed;
}
