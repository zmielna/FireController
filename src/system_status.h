#pragma once

namespace SystemStatus {
    void init();    // print the banner once, called at the end of setup()
    void update();  // call every loop() - reprints the banner whenever WiFi/
                     // MQTT/sensor status actually changes, not on a timer
}
