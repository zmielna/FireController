#pragma once

namespace Safety {
    void init();
    void update();

    bool isOverheat();
    bool isSensorFault();
    bool isSafe();
}
