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
  - `sensors/`, `safety/`, `display/`, `mqtt_handler/`, `button/`

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

MQTT Topic

firecontroller/status

Payload includes sensors, safety, system metrics, and button state.


If you want, I can also generate a **[detailed README](ca://s?q=Generate_detailed_README_for_FireController)** with wiring diagrams, installation steps, and Home Assistant integration.