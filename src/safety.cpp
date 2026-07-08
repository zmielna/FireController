#include "safety.h"
#include "sensors.h"

static bool overheat = false;
static bool sensorFault = false;

void Safety::init() {}

void Safety::update() {
    overheat = Sensors::getExhaustTemp() > 250.0f;
    sensorFault = false; // Placeholder
}

bool Safety::isOverheat() { return overheat; }
bool Safety::isSensorFault() { return sensorFault; }
