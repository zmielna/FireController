#pragma once

namespace Sensors {
    void init();
    void update();

    float getExhaustTemp();      // MAX31856, median-filtered over 5 samples
    float getExhaustTrend();     // approx deg C per ~minute, 0 until the trend buffer fills
    float getInletPressure();    // BMP280
    float getInletTemp();        // BMP280 temperature

    // These auto-retry every SENSOR_RETRY_MS instead of latching forever -
    // a transient I2C/SPI glitch no longer requires a reboot to recover from.
    bool  isMaxFault();
    bool  isBmpFault();
}
