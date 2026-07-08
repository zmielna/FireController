# FireController

FireController is an ESP32‑based control and monitoring system for a fireplace air‑intake actuator.  
It integrates thermocouple temperature sensing, pressure measurement, safety logic, MQTT telemetry, and an OLED UI.

---

## Features

- **Sensor drivers**  
  - MAX31856 (K‑type thermocouple)  
  - BMP280 (pressure + temperature)

- **Safety system**  
  - Overheat detection  
  - Sensor fault detection

- **OLED UI**  
  - Exhaust temperature  
  - Pressure  
  - Safety state  
  - MQTT + button status

- **MQTT telemetry**  
  - JSON status payload  
  - Automatic reconnect  
  - Home Assistant friendly

- **Modular architecture**  
  - `sensors/`, 
    `safety/`, 
    `display/`, 
    `mqtt_handler/`, 
    `button/`

---

## Project Structure

```text
FireController/
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── config.h
│   ├── sensors.h / sensors.cpp
│   ├── safety.h / safety.cpp
│   ├── display.h / display.cpp
│   ├── mqtt_handler.h / mqtt_handler.cpp
│   └── button.h / button.cpp
└── include/
```

---

## Requirements

PlatformIO libraries:

- ArduinoJson  
- Adafruit GFX  
- Adafruit SSD1306  
- Adafruit MAX31856  
- Adafruit BMP280  
- PubSubClient  

---

## Build & Upload

```bash
pio run
pio run -t upload
pio device monitor
```

## Gaps and issues

## Adding LED

Dioda LED RGB WS2812B (1 szt.) – jako szybki wskaźnik stanu:

- 🟢 normalna praca
- 🔵 rozpalanie
- 🟡 wygaszanie
- 🔴 alarm

### Critical — will block real operation

- No WiFi initialization
mqtt_handler.cpp uses WiFi and WiFi.RSSI(), but nothing calls WiFi.begin(). MQTT cannot connect without network setup.

- Missing PubSubClient dependency
The code includes <PubSubClient.h>, and the README lists it, but platformio.ini does not declare it in lib_deps. A clean build may fail unless it is added manually.

- No actuator control
The name and description refer to an “air-intake actuator,” but there is no relay, servo, PWM, or stepper logic. Today this is sense + display + publish, not control.

### Incomplete logic

- Safety sensor fault is a stub — sensorFault is hardcoded to false:

```
safety.cpp
Lines 9-12
void Safety::update() {
    overheat = Sensors::getExhaustTemp() > 250.0f;
    sensorFault = false; // Placeholder
}
```

- Sensors::isMaxFault() and isBmpFault() exist but are never used here or in the MQTT safety block.

- No safety response to overheat — overheat is detected and shown, but nothing shuts down or limits an actuator (since there isn’t one yet).

- No MQTT command handling — publish-only; no subscriptions for remote control.

### Minor / polish

- No button debouncing — raw digitalRead every loop.
- Display refreshes every loop — no rate limiting; fine at small scale, but unnecessary I2C traffic.
- No explicit Wire.begin(I2C_SDA, I2C_SCL) — often works on ESP32 via library defaults, but explicit init is safer.
- Hard-coded secrets/config — WiFi SSID/password and MQTT auth are missing from config.h (expected for now, but needed for deployment).
- README inconsistencies — mentions empty include/ folder; MQTT section formatting is broken; trailing typo in platformio.ini description (.o).
- No tests — no unit tests or hardware-in-loop checks.