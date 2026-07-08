#include "sensors.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX31856.h>
#include <Adafruit_BMP280.h>
#include <math.h>
#include <string.h>

// ---------------- MAX31856 ----------------
Adafruit_MAX31856 max31856(PIN_MAX31856_CS);

// ---------------- BMP280 ------------------
Adafruit_BMP280 bmp;

// ---------------- Internal state ----------
static float exhaustTempFiltered = NAN;
static float exhaustTrend = 0.0f;
static float inletPressure = NAN;
static float inletTemp = NAN;

// Faulted until the first successful init - safer default than "assume OK".
static bool maxFault = true;
static bool bmpFault = true;

static unsigned long lastMaxRetryMs = 0;
static unsigned long lastBmpRetryMs = 0;

// Median filter (5-sample window) - a single EMI spike or noisy reading
// shouldn't trip the safety thresholds in safety.cpp on its own.
static float flueHistory[5] = {0};
static uint8_t flueIdx = 0;
static bool historyFilled = false;

// Trend buffer: sampled once every ~10s independent of loop() rate, so the
// trend reflects real time elapsed rather than however fast loop() happens
// to spin. 6 samples * 10s = ~60s window.
static float trendBuffer[6] = {0};
static uint8_t trendIdx = 0;
static bool trendFilled = false;
static unsigned long lastTrendMs = 0;

static float medianOf5(const float* arr) {
    float sorted[5];
    memcpy(sorted, arr, sizeof(sorted));
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (sorted[j] < sorted[i]) {
                float t = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = t;
            }
        }
    }
    return sorted[2];
}

static bool initMax31856() {
    if (!max31856.begin()) {
        return false;
    }
    max31856.setThermocoupleType(MAX31856_TCTYPE_K);
    max31856.setNoiseFilter(MAX31856_NOISE_FILTER_50HZ);
    return true;
}

static bool initBmp280() {
    // Breakout boards differ: most default to 0x76, some (incl. many
    // Adafruit/GY-BMP280 clones with SDO pulled high) sit at 0x77.
    if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
        return false;
    }
    bmp.setSampling(
        Adafruit_BMP280::MODE_NORMAL,
        Adafruit_BMP280::SAMPLING_X2,   // temp
        Adafruit_BMP280::SAMPLING_X16,  // pressure
        Adafruit_BMP280::FILTER_X16,
        Adafruit_BMP280::STANDBY_MS_125
    );
    return true;
}

void Sensors::init() {
    Wire.begin(I2C_SDA, I2C_SCL);

    maxFault = !initMax31856();
    if (maxFault) Serial.println("MAX31856 init FAILED");

    bmpFault = !initBmp280();
    if (bmpFault) {
        Serial.println("BMP280 init FAILED - scanning I2C bus for connected devices:");
        int found = 0;
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                Serial.printf("  device found at 0x%02X\n", addr);
                found++;
            }
        }
        if (found == 0) {
            Serial.println("  no I2C devices found at all - check SDA/SCL wiring and power");
        }
    }

    unsigned long now = millis();
    lastMaxRetryMs = now;
    lastBmpRetryMs = now;
    lastTrendMs = now;
}

void Sensors::update() {
    unsigned long now = millis();

    // ---- MAX31856: retry periodically instead of latching the fault forever ----
    if (maxFault && (now - lastMaxRetryMs >= SENSOR_RETRY_MS)) {
        lastMaxRetryMs = now;
        maxFault = !initMax31856();
        if (!maxFault) Serial.println("MAX31856 recovered");
    }

    if (!maxFault) {
        float raw = max31856.readThermocoupleTemperature();
        uint8_t fault = max31856.readFault();

        if (fault) {
            // Adafruit_MAX31856 has no clearFault() - unlike the MAX31865 (RTD)
            // library, the SR/fault register here reflects live state on each
            // conversion, so there's nothing to explicitly clear. Just mark
            // faulted; the retry loop above will re-check on the next cycle.
            maxFault = true;
            exhaustTempFiltered = NAN;
            Serial.printf("MAX31856 fault: 0x%02X\n", fault);
        } else if (isnan(raw) || raw < TEMP_SENSOR_MIN_C || raw > TEMP_SENSOR_MAX_C) {
            maxFault = true;
            exhaustTempFiltered = NAN;
        } else {
            flueHistory[flueIdx % 5] = raw;
            flueIdx++;
            if (flueIdx >= 5) historyFilled = true;
            exhaustTempFiltered = historyFilled ? medianOf5(flueHistory) : raw;
        }
    } else {
        exhaustTempFiltered = NAN;
    }

    // ---- Trend, sampled on a fixed wall-clock cadence ----
    if (!isnan(exhaustTempFiltered) && (now - lastTrendMs >= 10000)) {
        lastTrendMs = now;
        trendBuffer[trendIdx % 6] = exhaustTempFiltered;
        trendIdx++;
        if (trendIdx >= 6) trendFilled = true;

        if (trendFilled) {
            float oldest = trendBuffer[trendIdx % 6];
            exhaustTrend = exhaustTempFiltered - oldest; // deg C per ~60s window
        } else {
            exhaustTrend = 0.0f;
        }
    }

    // ---- BMP280: same retry pattern ----
    if (bmpFault && (now - lastBmpRetryMs >= SENSOR_RETRY_MS)) {
        lastBmpRetryMs = now;
        bmpFault = !initBmp280();
        if (!bmpFault) Serial.println("BMP280 recovered");
    }

    if (!bmpFault) {
        float t = bmp.readTemperature();
        float p = bmp.readPressure() / 100.0f; // Pa -> hPa

        if (isnan(t) || isnan(p)) {
            bmpFault = true;
        } else {
            inletTemp = t;
            inletPressure = p;
        }
    }
}

float Sensors::getExhaustTemp() { return exhaustTempFiltered; }
float Sensors::getExhaustTrend() { return exhaustTrend; }
float Sensors::getInletPressure() { return inletPressure; }
float Sensors::getInletTemp() { return inletTemp; }

bool Sensors::isMaxFault() { return maxFault; }
bool Sensors::isBmpFault() { return bmpFault; }
