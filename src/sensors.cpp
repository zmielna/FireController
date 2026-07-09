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
static uint8_t bmpAddress = 0;   // 0 = not yet found; set on successful init

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

// Translates the MAX31856 fault byte into readable text. Bit meanings are
// from the Adafruit_MAX31856 header (MAX31856_FAULT_* constants) and the
// datasheet. Multiple bits are commonly set together - e.g. a disconnected
// thermocouple typically shows OPEN plus both TC range bits, since the ADC
// sees a floating/out-of-range input on top of the literal open circuit.
static void logMax31856Fault(uint8_t fault) {
    Serial.printf("MAX31856 fault: 0x%02X ->", fault);
    if (fault & MAX31856_FAULT_CJRANGE) Serial.print(" cold-junction-range");
    if (fault & MAX31856_FAULT_TCRANGE) Serial.print(" thermocouple-range");
    if (fault & MAX31856_FAULT_CJHIGH)  Serial.print(" cold-junction-high");
    if (fault & MAX31856_FAULT_CJLOW)   Serial.print(" cold-junction-low");
    if (fault & MAX31856_FAULT_TCHIGH)  Serial.print(" thermocouple-high");
    if (fault & MAX31856_FAULT_TCLOW)   Serial.print(" thermocouple-low");
    if (fault & MAX31856_FAULT_OVUV)    Serial.print(" over/under-voltage");
    if (fault & MAX31856_FAULT_OPEN)    Serial.print(" OPEN-CIRCUIT(not-connected?)");
    Serial.println();
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

// verbose=true prints the full diagnostic explanation (used once, from
// Sensors::init()). verbose=false stays silent on failure (used by the
// periodic background retry in Sensors::update()) - the retry still runs
// every SENSOR_RETRY_MS, but a sensor that's simply "not connected yet"
// shouldn't spam the log forever. SystemStatus::update() is the place that
// announces an actual OK<->FAULT transition, once, when it happens.
static bool initMax31856(bool verbose) {
    if (!max31856.begin()) {
        if (verbose) {
            Serial.println("MAX31856: begin() failed - check SPI wiring (CS/SCK/MISO/MOSI) and power");
        }
        return false;
    }
    max31856.setThermocoupleType(MAX31856_TCTYPE_K);
    max31856.setNoiseFilter(MAX31856_NOISE_FILTER_50HZ);

    // begin() cannot detect whether a chip is actually present - SPI has no
    // ACK mechanism the way I2C does, so it succeeds as long as the ESP32
    // can toggle the pins, chip attached or not. Self-test instead: read
    // back the register we just wrote, 3 times. A real chip echoes exactly
    // what was written; a floating/absent MISO line echoes noise, unlikely
    // to match 3 times running.
    bool selfTestOk = true;
    for (int i = 0; i < 3; i++) {
        if (max31856.getThermocoupleType() != MAX31856_TCTYPE_K) {
            selfTestOk = false;
            break;
        }
        delay(2);
    }

    if (!selfTestOk) {
        if (verbose) {
            Serial.println("MAX31856: self-test FAILED - SPI clocked without error, but register "
                            "readback doesn't match what was just written. No chip appears to be "
                            "actually responding (likely not connected yet).");
        }
        return false;
    }

    if (verbose) {
        Serial.println("MAX31856: initialized (K-type, 50Hz noise filter)");
    }
    return true;
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

    maxFault = !initMax31856(true);

    bmpFault = !initBmp280(true);

    Serial.printf("--- Sensor init done: MAX31856=%s  BMP280=%s ---\n",
        maxFault ? "FAULT" : "OK",
        bmpFault ? "FAULT" : "OK");

    unsigned long now = millis();
    lastMaxRetryMs = now;
    lastBmpRetryMs = now;
    lastTrendMs = now;
}

void Sensors::update() {
    unsigned long now = millis();

    // ---- MAX31856: retry periodically instead of latching the fault forever.
    //      Silent (verbose=false) - a still-disconnected sensor shouldn't log
    //      every 10s forever. SystemStatus reports the OK<->FAULT transition. ----
    if (maxFault && (now - lastMaxRetryMs >= SENSOR_RETRY_MS)) {
        lastMaxRetryMs = now;
        maxFault = !initMax31856(false);
    }

    if (!maxFault) {
        float raw = max31856.readThermocoupleTemperature();
        uint8_t fault = max31856.readFault();

        if (fault) {
            // Adafruit_MAX31856 has no clearFault() - unlike the MAX31865 (RTD)
            // library, the SR/fault register here reflects live state on each
            // conversion, so there's nothing to explicitly clear. Just mark
            // faulted; the retry loop above will re-check on the next cycle.
            // This DOES print (it's a runtime fault on a sensor that was
            // previously working, not a background retry on one that never
            // connected - worth knowing about immediately).
            maxFault = true;
            exhaustTempFiltered = NAN;
            logMax31856Fault(fault);
        } else if (isnan(raw) || raw < TEMP_SENSOR_MIN_C || raw > TEMP_SENSOR_MAX_C) {
            maxFault = true;
            exhaustTempFiltered = NAN;
            Serial.printf("MAX31856: reading %.1fC out of plausible range, treating as fault\n", raw);
        } else {
            bool wasFaulted = !historyFilled && flueIdx == 0; // first good reading after init
            flueHistory[flueIdx % 5] = raw;
            flueIdx++;
            if (flueIdx >= 5) historyFilled = true;
            exhaustTempFiltered = historyFilled ? medianOf5(flueHistory) : raw;
            if (wasFaulted) {
                Serial.printf("MAX31856: first valid reading %.1fC\n", raw);
            }
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

    // ---- BMP280: same silent-retry pattern ----
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
