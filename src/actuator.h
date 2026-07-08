#pragma once

namespace Actuator {
    void init();
    void update();

    void setDesiredPosition(float percent);  // 0-100, from MQTT or button
    float getDesiredPosition();              // last requested value, pre-safety
    float getCurrentPosition();              // actual applied value, post-safety
    bool  isSafetyOverridden();              // true if Safety changed the requested value
}
