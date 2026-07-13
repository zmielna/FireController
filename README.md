# Important Disclaimer

This device controls the physical airflow to a burning fireplace. The author is not responsible for any damage, fires, or malfunctions resulting from the use of this project. Use at your own risk, after verifying your installation.


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
│   ├── secrets.h.example
│   ├── sensors.h / sensors.cpp
│   ├── safety.h / safety.cpp
│   ├── actuator.h / actuator.cpp
│   ├── led_status.h / led_status.cpp
│   ├── display.h / display.cpp
│   ├── system_status.h / system_status.cpp
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

## Configuration

WiFi and MQTT connection details live in `src/secrets.h`, which is
gitignored so real credentials never end up committed. Only `secrets.h`
itself is ignored — `src/secrets.h.example` (a template with placeholder
values) is committed and tracked normally.

**First-time setup:**

```bash
cp src/secrets.h.example src/secrets.h
```

Then edit `src/secrets.h`:

```cpp
constexpr const char* WIFI_SSID = "YOUR_WIFI_SSID";
constexpr const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

constexpr const char* MQTT_HOST = "192.168.1.10";   // your broker's LAN IP
constexpr int MQTT_PORT = 1883;                     // 1883 = default, unencrypted

constexpr const char* MQTT_USER = "";               // leave "" for anonymous
constexpr const char* MQTT_PASSWORD_SECRET = "";    // leave "" for anonymous
```

- **`MQTT_HOST`** is normally the same machine running Home Assistant if
  you're using the Mosquitto add-on — check Settings → Add-ons → Mosquitto
  broker, or find your HA host's LAN IP directly.
- **Anonymous vs. authenticated**: most HA + Mosquitto setups require auth
  by default. If your broker rejects the connection, create a dedicated
  MQTT user in HA (Settings → People → Users — don't reuse your own login)
  and fill in `MQTT_USER`/`MQTT_PASSWORD_SECRET`. The firmware auto-detects
  which to use: empty strings connect anonymously, non-empty strings send
  credentials.

Repo already has a working `src/secrets.h` with placeholder values so it
compiles out of the box — but WiFi/MQTT obviously won't actually connect
until you put real values in.

**MQTT topics** (not secrets, safe to edit directly in `config.h`):

| Constant | Default | Purpose |
|---|---|---|
| `MQTT_TOPIC_STATUS` | `firecontroller/status` | Published every 2s — JSON with sensors, safety state, actuator position |
| `MQTT_TOPIC_SET_INTAKE` | `firecontroller/set/intake` | Subscribed — send `{"position": 0-100}` to command the damper |
| `MQTT_TOPIC_PHASE` | `firecontroller/phase` | Subscribed — plain text phase name from the HA automation, drives the status LED color |

If you change these, update the matching `mqtt:` entities in your Home
Assistant `configuration.yaml` to match, or discovery/state will silently
stop working with no error on either side.

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

### Reading MAX31856 fault codes

`Sensors::update()` decodes the raw fault byte into plain text, e.g.:

```text
MAX31856 fault: 0xFF -> cold-junction-range thermocouple-range cold-junction-high cold-junction-low thermocouple-high thermocouple-low over/under-voltage OPEN-CIRCUIT(not-connected?)
```

Seeing `OPEN-CIRCUIT` alongside a pile of other bits almost always just
means **the thermocouple isn't wired up yet** — an open circuit on the TC+/TC-
inputs also drags the cold-junction and range checks out of bounds, so you
get one real fault plus several derived ones. This is expected and harmless
while you're still waiting on parts; once the thermocouple is actually
connected, faults should stop appearing (or point at something more
specific, like a single `thermocouple-low`/`thermocouple-high` bit if the
polarity is reversed).

This particular decoded message only prints when a **previously-working**
sensor starts faulting during normal reads — not during the background
retry of a sensor that was never connected in the first place. See below
for why those two cases are treated differently.

### Why background retries stay quiet

The firmware retries `begin()` every `SENSOR_RETRY_MS` (10s default) while
a sensor is faulted, so it can recover automatically once you actually plug
something in — but those retries no longer print anything on failure.
Logging "still not there" every 10 seconds forever for a sensor you haven't
received yet is just noise. Only two things are logged:

- **The first attempt**, in `Sensors::init()` at boot — one clear message
  either way.
- **An actual state change**, via the Serial status banner
  (`system_status.cpp`), which reprints automatically the moment a sensor's
  fault status flips from OK to Missing or back. That's the single source
  of truth for "did anything change" — no need to watch for repeated
  per-retry messages.

If you want to see what a specific retry attempt is doing while actively
debugging wiring, that's what `initMax31856(bool verbose)` /
`initBmp280(bool verbose)` in `sensors.cpp` are for — temporarily pass
`true` from the retry call sites in `Sensors::update()` instead of `false`
if you need that level of detail for a session.

### Why the MAX31856 can report "initialized" with nothing connected at all

Worth understanding if you ever see `MAX31856: initialized` in the log
before the chip is actually wired up: **SPI has no acknowledgment
mechanism**, unlike I2C. An I2C `begin()` call fails cleanly if nothing
ACKs at that address; an SPI `begin()` call succeeds as long as the
microcontroller can toggle the CS/SCK/MOSI pins, whether or not a real chip
is on the other end of MISO. `Adafruit_MAX31856::begin()` has no ID/
signature register check to compensate for this, so it will return `true`
regardless of whether anything is actually connected.

With MISO left floating (no chip driving it), reads return whatever noise
that pin happens to pick up — usually enough to look like a fault most of
the time, but occasionally, by chance, the noise lines up with "no fault"
and produces a plausible-looking reading like `0.0°C`. That's what a
sequence like fault → fault → *valid 0.0°C reading* → fault usually means:
not a real recovery, just noise landing on a value that happens to pass
the checks for one cycle.

`initMax31856()` does a write-then-readback self-test: it writes the
thermocouple type register and immediately reads it back three times,
requiring an exact match every time. A real chip will echo back exactly
what was written; a floating MISO line is very unlikely to coincidentally
match three times in a row. This isn't a mathematically airtight guarantee
of physical presence — nothing purely software-side can be, given SPI's
lack of ACK — but it turns "always says OK" into "reliably says OK only
when something is actually answering."

### Why the OLED doesn't have the same problem

Unlike the MAX31856, the SSD1306 OLED is on I2C, which **does** have a real
ACK mechanism — a device either answers at its address or it doesn't, with
no ambiguity. `Display::init()` checks this directly
(`Wire.beginTransmission(OLED_ADDR)` / `Wire.endTransmission()`) before
even calling the display library's own `begin()`, since some versions of
`Adafruit_SSD1306::begin()` don't reliably surface an absent display as a
failure on their own. This check is a hard guarantee, not a heuristic —
no equivalent of the MAX31856's "probably not connected" uncertainty here.

---

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
- 🔵 lightning (from HA via `MQTT_TOPIC_PHASE`)
- 🟡 extinguishing (from HA via `MQTT_TOPIC_PHASE`)
- 🔴 blinking — any safety alarm tier (fault / high-limit / critical)
- 🟣 pulsing — MQTT lost

## Serial status banner

Printed once at the end of boot, and automatically reprinted whenever WiFi,
MQTT, or sensor status actually changes (not on a fixed timer) — so it
naturally captures things like WiFi connecting a few seconds before MQTT
does, or a sensor dropping out mid-run:

```text
================================================
FireController
Version  : 0.2.0
CPU      : ESP32-D0WD-V3 @ 240 MHz
Flash    : 4 MB
Heap     : 287 kB free
Scanning I2C...
  0x3C  SSD1306
  0x76  BMP280
OLED      OK
BMP280    OK
MAX31856  Missing
WiFi      Connected to YOUR_SSID (-52 dBm)
MQTT      Waiting
================================================
```

The I2C scan here is independent of `sensors.cpp`'s own scan-on-failure
logic — this one always runs, not just when something fails, so you get a
live view of exactly what's on the bus every time the banner reprints.
MAX31856 status isn't from this scan (it's SPI, not I2C) — it reflects the
write/readback self-test in `sensors.cpp`.

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

## To do

Adding second BME280 outside (atmospheric pressure reference) - without this, the "draft" measurement from one BME280 is useless, because we only measure the absolute pressure, but we are interested in the difference to the environment (typical chimney draft is 10-30 Pa, so it is the difference that counts, not the absolute value).


## License

See file LICENSE for the details of the license that covers use and reproduction of this code Apache License Version 2.0, January 2004