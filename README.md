# FireController

FireController is an ESP32‑based control and monitoring system for a fireplace air‑intake actuator.  
It integrates thermocouple temperature sensing, pressure measurement, safety logic, MQTT telemetry, and an OLED UI.

---

## Features

- **Sensor drivers**  
  - MAX31856 (K‑type thermocouple)  
  - BMP280 (pressure + temperature)

- **Actuator**  
  - Servo-driven cable damper, proportional 0-100%

- **Safety system**  
  - Graduated priority hierarchy (sensor fault → critical overheat →
    high-temp limit → MQTT lost → normal), evaluated locally every loop
    independent of WiFi/MQTT/HA

- **Status LED (WS2812B)**  
  - Combustion phase and alarm state at a glance

- **OLED UI**  
  - Exhaust temperature + trend  
  - Pressure  
  - Actuator position  
  - Safety state  
  - MQTT + button status

- **MQTT telemetry**  
  - JSON status payload  
  - Automatic reconnect  
  - Home Assistant friendly

- **Modular architecture**  
  - `sensors/`, 
    `safety/`, 
    `actuator/`, 
    `led_status/`, 
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
│   ├── actuator.h / actuator.cpp
│   ├── led_status.h / led_status.cpp
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
- ESP32Servo  
- FastLED

---

## Build & Upload

```bash
pio run
pio run -t upload
pio device monitor
```

---

## Wiring / Pinout

All pin numbers below come from `config.h` — that file is the single source
of truth; if you rewire something, change it there, not just here.

| Function | ESP32 pin | Notes |
|---|---|---|
| MAX31856 CS | GPIO5 | `PIN_MAX31856_CS` |
| MAX31856 SCK | GPIO18 | hardware VSPI default, not set explicitly in code |
| MAX31856 MISO | GPIO19 | hardware VSPI default |
| MAX31856 MOSI | GPIO23 | hardware VSPI default |
| BMP280 SDA | GPIO21 | `I2C_SDA`, shared bus with OLED |
| BMP280 SCL | GPIO22 | `I2C_SCL`, shared bus with OLED |
| OLED SDA | GPIO21 | same I2C bus as BMP280 |
| OLED SCL | GPIO22 | same I2C bus as BMP280 |
| Button | GPIO33 | `BUTTON_PIN`, internal pull-up enabled in firmware |
| Servo (damper) | GPIO13 | `SERVO_PIN`, PWM |
| WS2812B LED data | GPIO25 | `LED_PIN` |

### Power domains — read this before wiring anything

- **BMP280 and MAX31856 are 3.3V parts.** Feed them from the ESP32's 3.3V
  pin, not 5V, even if the breakout board silkscreen says "5V tolerant" —
  check your specific board's datasheet. Many cheap BMP280 breakouts have an
  onboard regulator and are fine on 5V, but the bare BMP280 chip is not, and
  not every board actually has that regulator despite looking similar.
- **Servo needs its own 5V/6V supply, not the ESP32's 3.3V rail or its USB
  5V pin.** A servo's stall/startup current (500mA-1A+ depending on size)
  will brown out the ESP32 if you power it from the same regulator. Common
  ground between the servo supply and the ESP32 is required; the two don't
  need a common positive rail.
- **WS2812B wants 5V on DIN.** The ESP32's 3.3V logic level is marginal for
  WS2812B's data-high threshold, especially over longer wires. If the LED
  shows wrong/flickering colors, add a 330-470Ω resistor in series on the
  data line, and/or a logic level shifter. Powering the LED itself from 5V
  is fine — only the data line's logic level is the concern.
- **I2C pull-ups**: most BMP280/OLED breakout boards already have onboard
  pull-up resistors on SDA/SCL. If you're wiring bare chips or your scan
  finds nothing despite correct wiring, add external 4.7kΩ pull-ups to
  3.3V on both lines.

### Debugging "sensor init FAILED" on boot

`Sensors::init()` now runs a full I2C bus scan and logs every address it
finds if BMP280 init fails — check `pio device monitor` output. A few
things that commonly cause this:

- **Wrong I2C address.** The firmware already tries both `0x76` and `0x77`
  for the BMP280 (some breakout boards default to one, some the other,
  depending on how the SDO pin is strapped). If the scan finds a device at
  neither address, it's not a firmware issue — it's wiring or power.
- **SDA/SCL swapped.** Easy to do since the labeling differs between
  breakout board silkscreens. The scan will simply find nothing if these
  are crossed.
- **No power to the sensor**, or a floating/unconnected ground.
- **Bus conflict**: if the OLED and BMP280 are on the same bus (they are,
  by design here) and one has a hardware fault, it can sometimes wedge the
  bus for the other device too. Disconnect one at a time to isolate.

If the scan finds devices but at unexpected addresses, update the address
list tried in `initBmp280()` (`sensors.cpp`) or `OLED_ADDR` (`config.h`)
accordingly.

---



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