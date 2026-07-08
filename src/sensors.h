#pragma once

namespace Sensors {
    void init();
    void update();

    float getExhaustTemp();      // MAX31856
    float getInletPressure();    // BMP280
    float getInletTemp();        // BMP280 temperature
    bool  isMaxFault();          // MAX31856 fault flag
    bool  isBmpFault();          // BMP280 fault flag
}
