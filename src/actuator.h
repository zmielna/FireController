#pragma once

namespace Actuator {
    void init();
    void update();

    void setDesired(bool open);
    void toggle();

    bool isOpen();
    bool isDesiredOpen();
    bool isSafetyBlocked();
}
