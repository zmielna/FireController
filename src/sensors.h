#pragma once

namespace Sensors {
    void init();
    void update();

    float getExhaustTemp();      // MAX6675, median-filtered over 5 samples
    float getExhaustTrend();     // approx deg C per ~minute, 0 until the trend buffer fills
    float getInletPressure();    // BMP280
    float getInletTemp();        // BMP280 temperature

    // isMaxFault() re-evaluates every update() call (a MAX6675 read is cheap
    // enough to just do every cycle). isBmpFault() auto-retries every
    // SENSOR_RETRY_MS instead of latching forever. Either way, a transient
    // fault no longer requires a reboot to recover from.
    bool  isMaxFault();
    bool  isBmpFault();
}
