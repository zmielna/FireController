#pragma once

// Thermocouple MAX31856
constexpr int PIN_MAX31856_CS = 5;

// BMP280 (I2C)
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;

// OLED SSD1306
constexpr int OLED_ADDR = 0x3C;

// Button (input-only GPIO, active low)
constexpr int BUTTON_PIN = 34;

// Air-intake actuator relay (active HIGH = open)
constexpr int ACTUATOR_PIN = 26;

// WiFi — set these before deploying
constexpr const char* WIFI_SSID = "YOUR_SSID";
constexpr const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// MQTT
constexpr const char* MQTT_HOST = "192.168.1.10";
constexpr int MQTT_PORT = 1883;
constexpr const char* MQTT_CLIENT_ID = "FireController";
constexpr const char* MQTT_TOPIC_STATUS = "firecontroller/status";
constexpr const char* MQTT_TOPIC_SET_INTAKE = "firecontroller/set/intake";

// Safety
constexpr float OVERHEAT_TEMP_C = 250.0f;

// Timing (ms)
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long DISPLAY_REFRESH_MS = 250;
constexpr unsigned long MQTT_PUBLISH_MS = 2000;
constexpr unsigned long WIFI_RETRY_MS = 5000;
constexpr unsigned long MQTT_RETRY_MS = 5000;
