#pragma once
#include <Arduino.h>

namespace MQTTHandler {
    void init();
    void update();
    void publishStatus();
    bool isConnected();
    String getPhase();   // last phase name received from HA via MQTT_TOPIC_PHASE
}
