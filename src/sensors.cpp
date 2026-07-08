#include "sensors.h"
#include "config.h"
#include <Arduino.h>
#include <Adafruit_MAX31856.h>
#include <Adafruit_BMP280.h>

// ---------------- MAX31856 ----------------
Adafruit_MAX31856 max31856(PIN_MAX31856_CS);

// ---------------- BMP280 ------------------
Adafruit_BMP280 bmp;

// ---------------- Internal state ----------
static float exhaustTemp = 0.0f;
static float inletPressure = 0.0f;
static float inletTemp = 0.0f;

static bool maxFault = false;
static bool bmpFault = false;

void Sensors::init() {
    // ---- MAX31856 ----
    if (!max31856.begin()) {
        Serial.println("MAX31856 init FAILED");
        maxFault = true;
    } else {
        max31856.setThermocoupleType(MAX31856_TCTYPE_K);
        max31856.setNoiseFilter(MAX31856_NOISE_FILTER_50HZ);
        maxFault = false;
    }

    // ---- BMP280 ----
    if (!bmp.begin(0x76)) {
        Serial.println("BMP280 init FAILED");
        bmpFault = true;
    } else {
        bmp.setSampling(
            Adafruit_BMP280::MODE_NORMAL,
            Adafruit_BMP280::SAMPLING_X2,   // temp
            Adafruit_BMP280::SAMPLING_X16,  // pressure
            Adafruit_BMP280::FILTER_X16,
            Adafruit_BMP280::STANDBY_MS_125
        );
        bmpFault = false;
    }
}

void Sensors::update() {
    // ---- MAX31856 ----
    if (!maxFault) {
        exhaustTemp = max31856.readThermocoupleTemperature();

        uint8_t fault = max31856.readFault();
        maxFault = (fault != 0);

        if (maxFault) {
            Serial.printf("MAX31856 fault: 0x%02X\n", fault);
            exhaustTemp = NAN;
        }
    }

    // ---- BMP280 ----
    if (!bmpFault) {
        inletTemp = bmp.readTemperature();
        inletPressure = bmp.readPressure() / 100.0f; // Pa → hPa

        if (isnan(inletTemp) || isnan(inletPressure)) {
            bmpFault = true;
            Serial.println("BMP280 read FAILED");
        }
    }
}

float Sensors::getExhaustTemp() { return exhaustTemp; }
float Sensors::getInletPressure() { return inletPressure; }
float Sensors::getInletTemp() { return inletTemp; }

bool Sensors::isMaxFault() { return maxFault; }
bool Sensors::isBmpFault() { return bmpFault; }
