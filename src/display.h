#pragma once

namespace Display {
    void init();
    void update();
    bool isOk();   // true if the OLED responded during init()
}
