# FireController

FireController is an ESP32‑based control and monitoring system for a fireplace air‑intake actuator.  
It integrates thermocouple temperature sensing, pressure measurement, safety logic, MQTT telemetry, and an OLED UI.

---

> **⚠️ Safety disclaimer**
>
> This project controls the physical air intake of a burning fireplace. It is a
> hobbyist DIY system, not a certified safety device, and has not been
> evaluated by any safety authority. The software includes multiple layers
> of fail-safe logic (see [Safety model](#safety-model) below), but software
> alone should never be your only line of defense around an open flame —
> an independent mechanical/thermal cutoff and a working smoke/CO detector
> in the room are not optional extras, they're required.
>
> **Use this project entirely at your own risk.** The author(s) accept no
> liability for damage, injury, fire, or loss of any kind arising from
> building, installing, or operating this system or any variation of it.
> If you build this, you are responsible for verifying your own
> installation, wiring, and fail-safe behavior before relying on it
> unattended. See [LICENSE](LICENSE) for the full legal disclaimer of
> warranty and liability.

---

## Features

- **Sensor drivers**  
  - MAX6675 (K‑type thermocouple)  
  - BMP280 (pressure + temperature)

- **Actuator**  
  - Servo-driven cable damper, proportional 0-100%

- **Safety system**  
  - Graduated priority hierarchy (sensor fault → critical overheat →
    high-temp limit → MQTT lost → normal), evaluated locally every loop
    independent of WiFi/MQTT/HA

- **Status LED (WS2812B)**  
  - Combustion phase and alarm state at a glance

- **OLED UI** (3 pages, rotating every 10s)  
  - Page 1: exhaust temperature + trend, pressure, actuator position,
    safety state, MQTT + button status  
  - Page 2: BMP280/MAX6675/WiFi/MQTT status + free heap — condensed version
    of the Serial status banner, for diagnosing without a laptop plugged in  
  - Page 3: board/chip info — firmware version, CPU model, clock, flash
    size, core count

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
- Adafruit SH110X  
- MAX6675 (RobTillaart)  
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
| MAX6675 CS (SELECT) | GPIO5 | `PIN_MAX6675_CS` |
| MAX6675 SCK (CLOCK) | GPIO18 | hardware VSPI default, not set explicitly in code |
| MAX6675 MISO (SO) | GPIO19 | hardware VSPI default — **needs an external 3.3V pull-up, see below** |
| MAX6675 MOSI | *(unused)* | MAX6675 only outputs data, MOSI isn't needed — SPI.begin() still reserves GPIO23 for it by ESP32 VSPI default, just leave it unconnected |
| BMP280 SDA | GPIO21 | `I2C_SDA`, shared bus with OLED |
| BMP280 SCL | GPIO22 | `I2C_SCL`, shared bus with OLED |
| OLED SDA | GPIO21 | same I2C bus as BMP280 |
| OLED SCL | GPIO22 | same I2C bus as BMP280 |
| Button | GPIO33 | `BUTTON_PIN`, internal pull-up enabled in firmware |
| Servo (damper) | GPIO13 | `SERVO_PIN`, PWM |
| WS2812B LED data | GPIO25 | `LED_PIN`, **needs a series resistor, see diagram below** |

### WS2812B wiring diagram

This is the one piece that isn't just "connect pin X to pin Y" — WS2812B
needs a specific arrangement to be reliable, not just electrically connected:

```text
 ESP32                              WS2812B
┌──────────┐                       ┌─────────┐
│      5V  ├───────────────────────┤ VCC     │
│          │                       │         │
│     GND  ├───────────┬───────────┤ GND     │
│          │            (shared    │         │
│  GPIO25  ├──[330-470Ω]───────────┤ DIN     │
└──────────┘   resistor            └─────────┘
```

- **VCC from 5V, not 3.3V.** WS2812B is a 5V part; running it from 3.3V
  under-drives it and colors will look dim/wrong.
- **GND is shared** between the ESP32 and the LED — this is required
  regardless of what powers the LED's VCC line.
- **The series resistor (330-470Ω) goes on the data line (GPIO25 → DIN),
  not on power.** It protects the LED's input from voltage spikes at
  power-up and helps clean up the signal edge — cheap insurance, not
  optional-feeling once you've had one glitch out.
- **Data direction matters**: WS2812B has an arrow on the PCB (or DIN/DOUT
  silkscreen labels) showing signal flow. GPIO25 connects to **DIN**, the
  input side — connecting to DOUT instead simply won't work.
- For a single LED (this project's case), you generally don't need the
  large buffer capacitor across VCC/GND that WS2812B *strips* require —
  that becomes relevant if you ever chain more LEDs off this one.

### Power domains — read this before wiring anything

- **BMP280 is a 3.3V part.** Feed it from the ESP32's 3.3V pin, not 5V,
  even if the breakout board silkscreen says "5V tolerant" — check your
  specific board's datasheet. Many cheap BMP280 breakouts have an onboard
  regulator and are fine on 5V, but the bare chip is not, and not every
  board actually has that regulator despite looking similar.
- **MAX6675: power it from 3.3V, not 5V — this matters for pin safety, not
  just signal cleanliness.** ESP32 GPIOs are **not 5V-tolerant** (max safe
  input is ~3.6V). The MAX6675 chip itself is rated for 3.0-5.5V supply
  (per the Maxim datasheet), and its `SO`/MISO output swings roughly
  between 0V and VCC. Power the module from ESP32's 3.3V pin instead of
  5V, and MISO's high level stays safely within the ESP32's input range
  with no extra components needed. This applies to the bare/minimal
  breakout modules typical for MAX6675 (VCC routed straight to the chip,
  no onboard regulator) — if your specific board turns out to have its own
  5V-only regulator on it (check the silkscreen for a 3-pin regulator IC;
  uncommon for MAX6675 breakouts but worth a 10-second look), you'd need a
  level shifter on MISO instead of powering at 3.3V directly.
- **MISO needs an external pull-up resistor (4.7kΩ–1kΩ, closer to 1kΩ for
  longer wire runs) between MISO and 3.3V** (not 5V — see above). This
  isn't optional polish — without it, an unconnected or
  momentarily-disconnected MAX6675 can produce noise on MISO that looks
  like plausible (but wrong) temperature data instead of cleanly reading
  as "not connected." With the pull-up in place, an absent/disconnected
  chip reliably reads back `0xFFFF`, which the firmware recognizes as
  `STATUS_NO_COMMUNICATION` — see the section below.
- **Servo needs its own 5V/6V supply, not the ESP32's 3.3V rail or its USB
  5V pin.** A servo's stall/startup current (500mA-1A+ depending on size)
  will brown out the ESP32 if you power it from the same regulator. Common
  ground between the servo supply and the ESP32 is required; the two don't
  need a common positive rail.
- **WS2812B wants 5V on VCC and DIN logic level is marginal at 3.3V**,
  especially over longer wires — see the wiring diagram above.
- **I2C pull-ups**: most BMP280/OLED breakout boards already have onboard
  pull-up resistors on SDA/SCL. If you're wiring bare chips or your scan
  finds nothing despite correct wiring, add external 4.7kΩ pull-ups to
  3.3V on both lines.

### Debugging "sensor init FAILED" on boot

`Sensors::init()` runs a full I2C bus scan and logs every address it finds
if BMP280 init fails — check `pio device monitor` output. A few things
that commonly cause this:

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

### Reading MAX6675 status codes

`Sensors::update()` calls `read()` every cycle and reacts to its status:

| Status | Meaning | Typical cause |
|---|---|---|
| `STATUS_OK` | Reading is valid | — |
| `STATUS_ERROR` | Thermocouple shorted to VCC | Wiring fault — check polarity/insulation |
| `STATUS_NO_COMMUNICATION` | Chip not responding | Not connected, or missing the MISO pull-up (see Power domains above) |
| `STATUS_NOREAD` | No read performed yet | Only seen before the very first read |

Unlike the earlier MAX31856-based design, this doesn't need a background
retry timer or a self-test heuristic — a MAX6675 read is a fast SPI
transaction (tens of microseconds), cheap enough to just perform every
`Sensors::update()` call, and `STATUS_NO_COMMUNICATION` is a real,
library-provided answer rather than something inferred indirectly. Logging
is still edge-triggered though: a message only prints on the transition
into or out of a fault, not every cycle while steady-state faulted — the
Serial status banner (`system_status.cpp`) remains the place to check for
current state, exactly as with the BMP280.

### Why the OLED doesn't have the same "always says OK" problem

Some I2C display libraries (this one included, historically) don't
reliably fail their `begin()` call when nothing is on the bus — the same
class of issue the old MAX31856 code had over SPI, just for a different
chip. Unlike SPI though, I2C has a real ACK mechanism: a device either
answers at its address or it doesn't, no ambiguity. `Display::init()`
checks this directly (`Wire.beginTransmission(OLED_ADDR)` /
`Wire.endTransmission()`) before ever calling the display library's own
`begin()`. This is a hard guarantee, not a heuristic.

### SH1106 vs SSD1306 — cosmetically identical modules, different chip

The actual panel used in this project is an **SH1106** (128x64, I2C),
driven by `Adafruit_SH110X` — not the more commonly-referenced SSD1306.
These modules are visually and electrically identical (same size, same
pinout, same default I2C address), so it's an easy substitution to make
by accident when ordering, and the two controllers are **not** command-compatible.

If you ever see a screen full of static/noise with a fragment of legible
text that looks mirrored or shifted, that's the signature of this exact
mismatch — the wrong driver library successfully talking to the chip (so
something renders) but mapping columns/segments incorrectly (so it comes
out scrambled). If that happens again after any hardware swap, check the
library in `platformio.ini` (`Adafruit_SH110X` vs `Adafruit_SSD1306`)
against the chip actually printed/implied on the board before assuming a
wiring fault.

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
- 🔵 `rozpalanie` (from HA via `MQTT_TOPIC_PHASE`)
- 🟡 `wygaszanie` (from HA via `MQTT_TOPIC_PHASE`)
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
  0x3C  SH1106/SSD1306
  0x76  BMP280
OLED      OK
BMP280    OK
MAX6675   Missing
WiFi      Connected to YOUR_SSID (-52 dBm)
MQTT      Waiting
================================================
```

The I2C scan here is independent of `sensors.cpp`'s own scan-on-failure
logic — this one always runs, not just when something fails, so you get a
live view of exactly what's on the bus every time the banner reprints.
MAX6675 status isn't from this scan (it's SPI, not I2C) — it reflects the
`read()` status code from `sensors.cpp`.

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

---

## License

Licensed under the [Apache License, Version 2.0](LICENSE). You're free to
use, modify, and distribute this project, including commercially, as long
as you retain the copyright/license notice and note what you changed in
any redistributed copies. See the [Safety disclaimer](#firecontroller) at
the top of this README and the Disclaimer of Warranty / Limitation of
Liability sections in [LICENSE](LICENSE) — both apply in full.

## Third-party licenses

This project depends on the following libraries via PlatformIO. None of
them require this project itself to use any particular license, but their
own terms still apply to their respective source code:

| Library | License |
|---|---|
| ArduinoJson | MIT |
| Adafruit GFX Library | BSD |
| Adafruit SH110X | BSD |
| MAX6675 (RobTillaart) | MIT |
| Adafruit BMP280 Library | BSD |
| PubSubClient | MIT |
| FastLED | MIT |
| ESP32Servo | **LGPL-2.1** |

`ESP32Servo` is the one worth knowing about specifically: LGPL normally
requires that a user be able to relink your application against a modified
version of the library, which is awkward to guarantee on a statically-linked
embedded target. Since this entire project is open source and buildable
from source via PlatformIO, that requirement is satisfied trivially —
anyone can already swap the library version and rebuild. This stops being
automatic if you ever fork this into a closed-source product; at that
point, LGPL compliance would need to be revisited properly (e.g. by
isolating the servo control behind a swappable module).