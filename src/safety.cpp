#include "safety.h"
#include "config.h"
#include "sensors.h"
#include <math.h>

static bool overheat = false;
static bool sensorFault = false;

void Safety::init() {}

void Safety::update() {
    float exhaustTemp = Sensors::getExhaustTemp();
    overheat = !isnan(exhaustTemp) && exhaustTemp > OVERHEAT_TEMP_C;
    sensorFault = Sensors::isMaxFault() || Sensors::isBmpFault();
}

bool Safety::isOverheat() { return overheat; }
bool Safety::isSensorFault() { return sensorFault; }
bool Safety::isSafe() { return !overheat && !sensorFault; }
