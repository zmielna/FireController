#pragma once

namespace LedStatus {
    void init();
    void update();  // call every loop() - handles blinking/pulsing internally
}
