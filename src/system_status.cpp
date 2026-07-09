#include "system_status.h"
#include "config.h"
#include "sensors.h"
#include "display.h"
#include "mqtt_handler.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

static bool lastWifiConnected = false;
static bool lastMqttConnected = false;
static bool lastMaxFault = true;
static bool lastBmpFault = true;
static bool lastOledOk = false;

static const char* i2cDeviceName(uint8_t addr) {
    switch (addr) {
        case 0x3C:
        case 0x3D: return "SSD1306";
        case 0x76:
        case 0x77: return "BMP280";
        case 0x38: return "AHT10/AHT20";
        default:   return "unknown";
    }
}

static void printBanner() {
    Serial.println("================================================");
    Serial.println("FireController");
    Serial.printf("Version  : %s\n", FIRMWARE_VERSION);
    Serial.printf("CPU      : %s @ %u MHz\n", ESP.getChipModel(), (unsigned)ESP.getCpuFreqMHz());
    Serial.printf("Flash    : %u MB\n", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
    Serial.printf("Heap     : %u kB free\n", (unsigned)(ESP.getFreeHeap() / 1024));

    Serial.println("Scanning I2C...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X  %s\n", addr, i2cDeviceName(addr));
            found++;
        }
    }
    if (found == 0) {
        Serial.println("  (nothing found)");
    }

    Serial.printf("OLED      %s\n", Display::isOk() ? "OK" : "Missing");
    Serial.printf("BMP280    %s\n", Sensors::isBmpFault() ? "Missing" : "OK");
    // MAX31856 is SPI, not on the I2C scan above - status comes from the
    // write/readback self-test in sensors.cpp, not from the bus scan.
    Serial.printf("MAX31856  %s\n", Sensors::isMaxFault() ? "Missing" : "OK");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi      Connected to %s (%d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
    } else {
        Serial.println("WiFi      Connecting...");
    }

    Serial.printf("MQTT      %s\n", MQTTHandler::isConnected() ? "Connected" : "Waiting");
    Serial.println("================================================");
}

void SystemStatus::init() {
    printBanner();
    lastWifiConnected = WiFi.status() == WL_CONNECTED;
    lastMqttConnected = MQTTHandler::isConnected();
    lastMaxFault = Sensors::isMaxFault();
    lastBmpFault = Sensors::isBmpFault();
    lastOledOk = Display::isOk();
}

void SystemStatus::update() {
    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    bool mqttConnected = MQTTHandler::isConnected();
    bool maxFault = Sensors::isMaxFault();
    bool bmpFault = Sensors::isBmpFault();
    bool oledOk = Display::isOk();

    if (wifiConnected != lastWifiConnected ||
        mqttConnected != lastMqttConnected ||
        maxFault != lastMaxFault ||
        bmpFault != lastBmpFault ||
        oledOk != lastOledOk) {
        printBanner();
        lastWifiConnected = wifiConnected;
        lastMqttConnected = mqttConnected;
        lastMaxFault = maxFault;
        lastBmpFault = bmpFault;
        lastOledOk = oledOk;
    }
}
