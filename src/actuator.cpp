#include "actuator.h"
#include "config.h"
#include "safety.h"
#include <Arduino.h>

static bool desiredOpen = false;
static bool actualOpen = false;
static bool safetyBlocked = false;

void Actuator::init() {
    pinMode(ACTUATOR_PIN, OUTPUT);
    digitalWrite(ACTUATOR_PIN, LOW);
}

void Actuator::update() {
    safetyBlocked = !Safety::isSafe();
    actualOpen = desiredOpen && !safetyBlocked;
    digitalWrite(ACTUATOR_PIN, actualOpen ? HIGH : LOW);
}

void Actuator::setDesired(bool open) {
    desiredOpen = open;
}

void Actuator::toggle() {
    desiredOpen = !desiredOpen;
}

bool Actuator::isOpen() { return actualOpen; }
bool Actuator::isDesiredOpen() { return desiredOpen; }
bool Actuator::isSafetyBlocked() { return safetyBlocked; }
