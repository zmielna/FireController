#pragma once

// Thermocouple MAX31856
constexpr int PIN_MAX31856_CS = 5;

// BMP280 (I2C)
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;

// OLED SSD1306
constexpr int OLED_ADDR = 0x3C;

// Button
constexpr int BUTTON_PIN = 34;

// MQTT
constexpr const char* MQTT_HOST = "192.168.1.10";
constexpr int MQTT_PORT = 1883;
