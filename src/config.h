#pragma once

// ---------------- Thermocouple MAX31856 ----------------
constexpr int PIN_MAX31856_CS = 5;

// ---------------- BMP280 (I2C) ----------------
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;

// ---------------- OLED SSD1306 ----------------
constexpr int OLED_ADDR = 0x3C;

// ---------------- Button ----------------
// GPIO34 was the original pin, but GPIO34-39 on ESP32 are input-only and
// have NO internal pull-up/pull-down circuitry. INPUT_PULLUP silently does
// nothing on those pins, leaving the input floating. Moved to GPIO33, which
// supports internal pulls. If you rewire back to 34+, add an external
// ~10k pull-up resistor to 3.3V.
constexpr int BUTTON_PIN = 33;

// ---------------- Damper servo (cable-actuated, 40mm throw) ----------------
constexpr int SERVO_PIN = 13;
// Calibrate these two on the bench: attach the cable, find the pulse width
// where the damper is fully closed (0%) and fully open (100% / 40mm pull).
// Do NOT assume 1000/2000 is correct for your servo+horn+cable geometry -
// verify with the damper disconnected first, then with it attached, checking
// it doesn't bind or overtravel at either end.
constexpr int SERVO_MIN_US = 1000;   // 0%   - fully closed
constexpr int SERVO_MAX_US = 2000;   // 100% - 40mm pulled

// ---------------- WS2812B status LED ----------------
constexpr int LED_PIN = 25;
constexpr int LED_BRIGHTNESS = 60;   // 0-255

// ---------------- Safety thresholds (graduated) ----------------
// Priority order enforced in safety.cpp:
//   sensor fault > critical overheat > high-temp limit > MQTT lost > normal
constexpr float TEMP_SENSOR_MIN_C = -20.0f;
constexpr float TEMP_SENSOR_MAX_C = 900.0f;
constexpr float TEMP_HIGH_LIMIT_C = 320.0f;   // cap max opening (soft ceiling)
constexpr float TEMP_CRITICAL_C = 380.0f;     // hard close to MIN_SAFE_POSITION_PCT
constexpr float MIN_SAFE_POSITION_PCT = 25.0f;      // never fully closed even in critical state
constexpr float HIGH_LIMIT_MAX_POSITION_PCT = 40.0f; // ceiling while in HIGH_TEMP_LIMIT
constexpr float STARTUP_POSITION_PCT = 100.0f;       // fail-open default: boot, sensor fault

// ---------------- MQTT-lost local fallback ----------------
constexpr unsigned long MQTT_LOST_TIMEOUT_MS = 300000;  // 5 min without a message
constexpr float MQTT_LOST_HOT_THRESHOLD_C = 150.0f;     // above this = fire is active
constexpr float MQTT_LOST_POSITION_HOT_PCT = 60.0f;     // moderate opening, don't rush the fire
constexpr float MQTT_LOST_POSITION_COLD_PCT = 100.0f;   // cold/unknown = safe to fully open

// ---------------- Sensor fault retry ----------------
constexpr unsigned long SENSOR_RETRY_MS = 10000; // attempt re-init every 10s while faulted

// ---------------- WiFi — set these before deploying ----------------
constexpr const char* WIFI_SSID = "YOUR_SSID";
constexpr const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// ---------------- MQTT ----------------
constexpr const char* MQTT_HOST = "192.168.1.10";
constexpr int MQTT_PORT = 1883;
constexpr const char* MQTT_CLIENT_ID = "FireController";
constexpr const char* MQTT_TOPIC_STATUS = "firecontroller/status";
constexpr const char* MQTT_TOPIC_SET_INTAKE = "firecontroller/set/intake";   // {"position": 0-100}
constexpr const char* MQTT_TOPIC_PHASE = "firecontroller/phase";            // plain text phase name from HA

// ---------------- Timing (ms) ----------------
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long DISPLAY_REFRESH_MS = 250;
constexpr unsigned long MQTT_PUBLISH_MS = 2000;
constexpr unsigned long WIFI_RETRY_MS = 5000;
constexpr unsigned long MQTT_RETRY_MS = 5000;
