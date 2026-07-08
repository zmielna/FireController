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

## Actuator

Cable-actuated damper (Jotul i570 fresh-air intake), servo-driven, 40mm
throw between 0% and 100%. Calibrate `SERVO_MIN_US`/`SERVO_MAX_US` in
`config.h` on the bench before connecting the cable.

## Safety model

Graduated priority hierarchy, evaluated locally on the ESP32 every loop,
independent of WiFi/MQTT/HA:

1. **Sensor fault** → fail OPEN (100%). An unknown reading is never treated
   as safe-to-restrict.
2. **Critical overheat** (≥380°C) → hard close to `MIN_SAFE_POSITION_PCT`
   (never fully closed).
3. **High-temp limit** (≥320°C) → soft ceiling on max opening, doesn't
   override a lower value already requested.
4. **MQTT lost** (>5 min silence) → moderate opening if the fire is active,
   full open if cold/unknown.
5. **Normal** → HA/manual control has full authority.

## Status LED (WS2812B)

- 🟢 normal operation (or phase not yet known)
- 🔵 `rozpalanie` (from HA via `MQTT_TOPIC_PHASE`)
- 🟡 `wygaszanie` (from HA via `MQTT_TOPIC_PHASE`)
- 🔴 blinking — any safety alarm tier (fault / high-limit / critical)
- 🟣 pulsing — MQTT lost

## Known gaps

- **No AUTO/MANUAL lock.** Button presses and MQTT position commands both
  write to the same `desiredPosition` — whichever arrives last wins. A
  physical button press can be silently overwritten by the next HA update.
  Needs an explicit mode flag (long-press to toggle, as originally discussed)
  before this is safe to rely on day-to-day.
- **No OLED screen rotation / long-press handling** — single static screen,
  no AUTO/MANUAL indication on-device.
- **No Preferences (NVS) persistence** — mode and last position are lost on
  reboot; always restarts fail-open at 100%.
- **PubSubClient is synchronous** — `mqttClient.loop()` blocks briefly on
  network I/O; fine at this scale, but a stall on a bad connection will
  delay `Sensors::update()`/`Safety::update()` in the same loop iteration.
  The graduated safety checks still run every loop and are not skipped by
  this, but their *cadence* can jitter under network stress.
- **No tests** — no unit tests or hardware-in-loop checks.