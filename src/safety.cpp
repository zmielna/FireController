#include "safety.h"
#include "config.h"
#include "sensors.h"
#include "mqtt_handler.h" // <--- DODANY NAGŁÓWEK
#include <Arduino.h>
#include <math.h>

static SafetyState state = SafetyState::NORMAL;
static float enforcedPosition = -1.0f;
static float positionCeiling = -1.0f;
static unsigned long lastMqttSeenMs = 0;

void Safety::init() {
    // Start the MQTT timer at boot, not at zero - otherwise a slow WiFi
    // handshake on a cold boot would look identical to "5 minutes with no HA".
    lastMqttSeenMs = millis();
}

void Safety::notifyMqttSeen() {
    lastMqttSeenMs = millis();
}

void Safety::update() {
    // Jeśli MQTT jest aktywne i połączone, odświeżamy timer aktywności
    if (MQTTHandler::isConnected()) {
        lastMqttSeenMs = millis();
    }

    float exhaust = Sensors::getExhaustTemp();

    bool sensorFault = Sensors::isMaxFault() || Sensors::isBmpFault() ||
                        isnan(exhaust) ||
                        exhaust < TEMP_SENSOR_MIN_C || exhaust > TEMP_SENSOR_MAX_C;

    // Priority 1: sensor fault -> FAIL OPEN.
    if (sensorFault) {
        state = SafetyState::SENSOR_FAULT;
        enforcedPosition = STARTUP_POSITION_PCT;
        positionCeiling = -1.0f;
        return;
    }

    // Priority 2: critical overheat -> hard close toward the safe minimum.
    if (exhaust >= TEMP_CRITICAL_C) {
        state = SafetyState::CRITICAL_TEMP;
        enforcedPosition = MIN_SAFE_POSITION_PCT;
        positionCeiling = -1.0f;
        return;
    }

    // Priority 3: high temp -> soft ceiling, not a hard override.
    if (exhaust >= TEMP_HIGH_LIMIT_C) {
        state = SafetyState::HIGH_TEMP_LIMIT;
        enforcedPosition = -1.0f;
        positionCeiling = HIGH_LIMIT_MAX_POSITION_PCT;
        return;
    }

    // Priority 4: MQTT/HA lost beyond timeout -> local fallback.
    if (millis() - lastMqttSeenMs > MQTT_LOST_TIMEOUT_MS) {
        state = SafetyState::MQTT_LOST;
        enforcedPosition = (exhaust > MQTT_LOST_HOT_THRESHOLD_C)
                                ? MQTT_LOST_POSITION_HOT_PCT
                                : MQTT_LOST_POSITION_COLD_PCT;
        positionCeiling = -1.0f;
        return;
    }

    state = SafetyState::NORMAL;
    enforcedPosition = -1.0f;
    positionCeiling = -1.0f;
}

SafetyState Safety::getState() { return state; }
float Safety::getEnforcedPosition() { return enforcedPosition; }
float Safety::getPositionCeiling() { return positionCeiling; }

bool Safety::isSafe() { return state == SafetyState::NORMAL; }
bool Safety::isOverheat() {
    return state == SafetyState::CRITICAL_TEMP || state == SafetyState::HIGH_TEMP_LIMIT;
}
bool Safety::isSensorFault() { return state == SafetyState::SENSOR_FAULT; }

const char* Safety::stateToString(SafetyState s) {
    switch (s) {
        case SafetyState::NORMAL:          return "ok";
        case SafetyState::SENSOR_FAULT:    return "sensor_fault";
        case SafetyState::HIGH_TEMP_LIMIT: return "high_temp_limit";
        case SafetyState::CRITICAL_TEMP:   return "critical_overheat";
        case SafetyState::MQTT_LOST:       return "mqtt_lost";
    }
    return "unknown";
}