#pragma once

enum class SafetyState {
    NORMAL,
    SENSOR_FAULT,
    HIGH_TEMP_LIMIT,
    CRITICAL_TEMP,
    MQTT_LOST
};

namespace Safety {
    void init();
    void update();

    SafetyState getState();
    const char* stateToString(SafetyState s);

    // Back-compat helpers used by display.cpp / mqtt_handler.cpp
    bool isSafe();          // true only in NORMAL
    bool isOverheat();      // true in HIGH_TEMP_LIMIT or CRITICAL_TEMP
    bool isSensorFault();   // true in SENSOR_FAULT

    // Hard override: Actuator must set exactly this position. -1 = no override.
    float getEnforcedPosition();

    // Soft ceiling: Actuator may go up to this value but no higher. -1 = no cap.
    float getPositionCeiling();

    // Call whenever a valid MQTT message is received (any topic), so the
    // MQTT_LOST fallback timer resets independent of what the message said.
    void notifyMqttSeen();
}
