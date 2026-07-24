#include "sensors.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <MAX6675.h>
#include <Adafruit_BMP280.h>
#include <math.h>
#include <string.h>

// ---------------- MAX6675 (K-type thermocouple, hardware SPI) ----------------
MAX6675 thermocouple(PIN_MAX6675_CS, &SPI);

// ---------------- BMP280 ------------------
Adafruit_BMP280 bmp;
static uint8_t bmpAddress = 0;   // 0 = not yet found; set on successful init

// ---------------- Internal state ----------
static float exhaustTempFiltered = NAN;
static float exhaustTrend = 0.0f;
static float inletPressure = NAN;
static float inletTemp = NAN;

// Faulted until the first successful read - safer default than "assume OK".
static bool maxFault = true;
static bool bmpFault = true;

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

// MAX6675 (via RobTillaart's library) exposes 4 status codes from read():
// STATUS_OK(0), STATUS_ERROR(4, thermocouple shorted to VCC),
// STATUS_NOREAD(128, nothing read yet), STATUS_NO_COMMUNICATION(129, chip
// not responding - typically "not connected"). This is genuinely better
// fault granularity than the datasheet alone suggests - the library
// distinguishes a wiring short from a missing chip, which the earlier
// MAX31856 self-test approach couldn't do at all for MAX6675-class parts.
static void logMax6675Status(uint8_t status) {
    switch (status) {
        case STATUS_ERROR:
            Serial.println("MAX6675: thermocouple SHORT TO VCC - check wiring/polarity");
            break;
        case STATUS_NO_COMMUNICATION:
            Serial.println("MAX6675: no communication - chip not responding "
                            "(not connected, or missing the MISO pull-up resistor - see README)");
            break;
        case STATUS_NOREAD:
            Serial.println("MAX6675: no read performed yet");
            break;
        default:
            Serial.printf("MAX6675: unknown status 0x%02X\n", status);
    }
}

// Scans the I2C bus and logs every address that ACKs. Called once, on
// BMP280 init failure - not on every retry, to avoid flooding.
static void scanI2CBus() {
    Serial.println("  scanning I2C bus:");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("    device found at 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0) {
        Serial.println("    no I2C devices found at all - check SDA/SCL wiring and power");
    }
}

static bool initBmp280(bool verbose) {
    // Breakout boards differ: most default to 0x76, some (incl. many
    // Adafruit/GY-BMP280 clones with SDO pulled high) sit at 0x77.
    if (bmp.begin(0x76)) {
        bmpAddress = 0x76;
    } else if (bmp.begin(0x77)) {
        bmpAddress = 0x77;
    } else {
        bmpAddress = 0;
        if (verbose) {
            Serial.println("BMP280: begin() failed at both 0x76 and 0x77");
            scanI2CBus();
        }
        return false;
    }

    bmp.setSampling(
        Adafruit_BMP280::MODE_NORMAL,
        Adafruit_BMP280::SAMPLING_X2,   // temp
        Adafruit_BMP280::SAMPLING_X16,  // pressure
        Adafruit_BMP280::FILTER_X16,
        Adafruit_BMP280::STANDBY_MS_125
    );
    if (verbose) {
        Serial.printf("BMP280: initialized at 0x%02X\n", bmpAddress);
    }
    return true;
}

void Sensors::init() {
    Wire.begin(I2C_SDA, I2C_SCL);

    Serial.println("--- Sensor init ---");

    // MAX6675's begin() returns void - it can't tell us whether a chip is
    // actually present, only set up the pins. Presence/fault detection
    // happens via read()'s status code instead, checked below and on every
    // subsequent update() call.
    SPI.begin(); // must be called before thermocouple.begin() (library requirement)
    thermocouple.begin();

    uint8_t maxStatus = thermocouple.read();
    maxFault = (maxStatus != STATUS_OK);
    if (maxFault) {
        logMax6675Status(maxStatus);
    } else {
        Serial.printf("MAX6675: initialized, first reading %.1fC\n", thermocouple.getCelsius());
    }

    bmpFault = !initBmp280(true);

    Serial.printf("--- Sensor init done: MAX6675=%s  BMP280=%s ---\n",
        maxFault ? "FAULT" : "OK",
        bmpFault ? "FAULT" : "OK");

    unsigned long now = millis();
    lastBmpRetryMs = now;
    lastTrendMs = now;
}

void Sensors::update() {
    unsigned long now = millis();

    // ---- MAX6675: a read() is a fast SPI transaction (tens of microseconds),
    //      unlike MAX31856's begin()-based self-test, so there's no need for
    //      a retry timer here - just read every cycle and react to the
    //      status. Logging is edge-triggered (only on OK<->FAULT
    //      transitions), matching the "SystemStatus is the source of truth
    //      for steady state" design used elsewhere in this file. ----
    uint8_t status = thermocouple.read();

    if (status == STATUS_OK) {
        float raw = thermocouple.getCelsius();
        if (isnan(raw) || raw < TEMP_SENSOR_MIN_C || raw > TEMP_SENSOR_MAX_C) {
            if (!maxFault) {
                Serial.printf("MAX6675: reading %.1fC out of plausible range, treating as fault\n", raw);
            }
            maxFault = true;
            exhaustTempFiltered = NAN;
        } else {
            bool wasFaulted = maxFault;
            flueHistory[flueIdx % 5] = raw;
            flueIdx++;
            if (flueIdx >= 5) historyFilled = true;
            exhaustTempFiltered = historyFilled ? medianOf5(flueHistory) : raw;
            maxFault = false;
            if (wasFaulted) {
                Serial.printf("MAX6675: first valid reading %.1fC\n", raw);
            }
        }
    } else {
        if (!maxFault) {
            // just transitioned from OK to faulted - worth logging immediately,
            // as opposed to a sensor that was never connected in the first place
            logMax6675Status(status);
        }
        maxFault = true;
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

    // ---- BMP280: silent-retry pattern (begin() is heavier than a plain
    //      read, so this one still uses the SENSOR_RETRY_MS gate) ----
    if (bmpFault && (now - lastBmpRetryMs >= SENSOR_RETRY_MS)) {
        lastBmpRetryMs = now;
        bmpFault = !initBmp280(false);
    }

    if (!bmpFault) {
        float t = bmp.readTemperature();
        float p = bmp.readPressure() / 100.0f; // Pa -> hPa

        if (isnan(t) || isnan(p)) {
            bmpFault = true;
            Serial.println("BMP280: read returned NaN, treating as fault");
        } else {
            if (isnan(inletTemp)) {
                Serial.printf("BMP280: first valid reading %.1fC / %.1f hPa\n", t, p);
            }
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
