# ESP 6-Digit Numitron Clock

Firmware and hardware design for a **six-digit Numitron clock** built around a **Wemos D1 Mini
(ESP8266)**. The clock keeps time from NTP with a DS1307 RTC as backup, drives the tubes through a
48-bit constant-current shift-register chain with PWM dimming, segment pre-heat and filament-lag
smoothing, lights per-digit WS2812B backlights, and is configured either from a single front-panel
button or from a built-in web interface.

This repository holds both sides of the project; the firmware in `Software/` is the main body of
work and is the focus of this document. The same firmware drives all three board variants in
`Hardware/`.

---

## Repository Layout

```
.
├── LICENSE            Non-commercial licence covering the whole repository
├── Software/          ESP8266 firmware (PlatformIO + Arduino)   ← the focus of this README
│   ├── src/           Implementation — ESP8266Clock.ino plus the component .cpp files
│   ├── include/       Headers, mode constants, segment tables, pin allocations
│   ├── platformio.ini Build environment (env: d1_mini)
│   ├── README.md      User guide: boot sequence, button, display modes, settings menu, web UI
│   └── SPECIFICATION.md  Technical reference: GPIO, display pipeline, config JSON schema, timings
└── Hardware/          KiCad projects — one per tube type, all pin-compatible with the firmware
    ├── da2000-6-digit/  DA-2000 tubes, 6× TLC5916 drivers (schematic + dimension PDFs)
    ├── iv13-6-digit/    IV-13 tubes, 6× TLC5916 drivers
    └── iv9-6-digit/     IV-9 tubes, 3× DM13A drivers (schematic + dimension PDFs)
```

---

## Quick Start

The firmware is a [PlatformIO](https://platformio.org/) project. Everything below runs from the
`Software/` directory.

```bash
cd Software

pio run                      # build (default env: d1_mini)
pio run -t upload            # flash the firmware
pio run -t monitor           # serial monitor, 115200 baud
```

There is **no filesystem image to upload**. Unlike the sibling Nixie project, every web page is
generated in C++ at request time (`HtmlServer.cpp` plus the page handlers in `ESP8266Clock.ino`),
so SPIFFS holds only the two JSON files the clock writes itself. A fresh board comes up with a
working web UI immediately after `upload`.

Build environment (`platformio.ini`):

| Setting | Value |
|---------|-------|
| Platform / board | `espressif8266` / `d1_mini` |
| Framework | `arduino` |
| CPU | 160 MHz (`board_build.f_cpu`) |
| Filesystem | SPIFFS — `/config.json` and `/stats.json` |
| Monitor port | `/dev/cu.usbserial-110` (macOS; change for your adapter) |
| Monitor speed | 115200 baud, RTS/DTR held off so opening the monitor does not reset the board |

Serial debug output is off by default (`debugVal` in `setup()`); it can be switched on at runtime
by visiting `/debug`, which enables `DebugManager` and redirects back to the utility page.

### Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| `paulstoffregen/Time` | ^1.6.1 | Timekeeping and time arithmetic |
| `me-no-dev/ESPAsyncUDP` | alpha (pinned SHA) | Asynchronous NTP client |
| `bblanchon/ArduinoJson` | 5.13.4 | Config persistence (note: v5 API) |
| `makuna/NeoPixelBus` | ^2.7.5 | WS2812B backlight driver (DMA on GPIO3) |
| `tzapu/WiFiManager` | ^0.16.0 | WiFi configuration captive portal |
| `adafruit/Adafruit GFX Library` | ^1.11.5 | Graphics primitives for the optional OLED |
| `adafruit/Adafruit SSD1306` | ^2.5.7 | Optional status OLED |

> `OLED.cpp` / `OLED.h` are complete but are **not** currently included by `ESP8266Clock.ino` —
> the status OLED is dormant code, and the two Adafruit dependencies exist to keep it compiling.

---

## Firmware Architecture

A single cooperative main loop runs at roughly 100 Hz (a fixed `delay(10)` at the bottom). There
is no display ISR: the whole 48-bit frame is shifted out once per loop pass, and the loop rate is
therefore the display refresh rate. The clock counts its own passes and reports them as
**impressions per second** on the status page and in temporary display mode 5 — a direct health
indicator, since HTTP work steals refreshes.

```
setup()
  OutputManager → NTP component → DebugManager → LEDManager
  → I2C (GPIO4/GPIO5) → button held at boot? → factory reset
  → load /config.json from SPIFFS → optional factory test sequence
  → WiFiManager autoConnect (saved network → AP portal "SetMeUp!", 60 s timeout)
  → NTP (10 s retry window) → web server + /update + mDNS
  → PIR pull-up → RTC detect and seed → show version → load stats
  Each stage paints a POST message on the tubes and a colour on the diagnostic LEDs
  (green = ok, yellow = in progress, red = failed, blue = reset performed).

loop()  ~10 ms
  server.handleClient(), mdns.update()
  on second change    → performOncePerSecondProcessing()
  on minute / hour / day boundary → stats, RTC re-read, reconnect, save stats
  button1.checkButton()  → mode preview on 1 s / 2 s hold, commit on release
  nextMode != currentMode ? setNextMode() : processCurrentMode()
  PIR sample → LDR read → outputManager.outputDisplay() → setLeds()
```

### The display pipeline

`OutputManager::outputDisplay()` runs once per loop pass and builds the frame in stages:

1. **Blank / blink** — per-digit display type (`BLANKED`, `NORMAL`, `BLINK`); the blink duty cycle
   is deliberately asymmetric (70 impressions on, 55 off) because Numitron filaments light slowly.
2. **Smoothing** — when a digit changes, the old and new segment patterns are OR'd together for
   `_tubeLag` milliseconds, so the filament cross-fades instead of snapping. The lag is derived
   from the ambient light reading by a quadratic fit and scaled by the pre-heat setting.
3. **Indicators** — the 8th bit of each digit's driver (`SEG_IND`) carries the separator "colon"
   LEDs on digits 1–4 and the left/right status LEDs on digits 0 and 5. Optional alternate-frame
   dimming halves their brightness.
4. **Pre-heat** — every *n*th frame, all segments are forced on (`0x7f`) to keep cold filaments
   warm; *n* is the pre-heat strength, 1 (very strong) to 5 (off).
5. **Shift out** — two 24-bit words are clocked out MSB-first and latched (`shiftOut48`).
6. **Global brightness** — the BLANK/OE pin is driven by hardware PWM (`analogWrite`, range 1023)
   from the smoothed LDR value.

### Module Map

| Module | Files | Responsibility |
|--------|-------|----------------|
| Main sketch | `ESP8266Clock.ino` (~2.8 kLOC) | Setup, main loop, mode state machine, button dispatch, all web page handlers |
| Output manager | `OutputManagerNumi6.*` (~0.9 kLOC) | Singleton display buffer, segment tables, blank/blink/smooth/pre-heat, 48-bit shift out, PWM dimming |
| LED manager | `LEDManager.*` | NeoPixel backlight and underlight strategies, gamma curve, LDR dimming, seconds pulse |
| Config storage | `SPIFFS.*` | `/config.json` and `/stats.json` read/write, factory defaults |
| HTML server | `HtmlServer.*` | Bootstrap page scaffolding, form widgets, argument parsing, auth helpers |
| NTP manager | `NtpAsync.*` | Async UDP NTP, POSIX TZ handling, update scheduling, millis-overflow handling |
| RTC manager | `ESP_DS1307.*` | DS1307 over I2C, BCD conversion |
| Transitions | `DA2000-Transition.*` | "Slots" date effects — wipe in/out and bang in/out |
| Buttons | `ClockButton.*` | Debounce (5 consecutive reads) and press-duration state machine |
| Utilities | `ClockUtils.*` | IP and time formatting, string/number parsing, hex/BCD helpers |
| Globals / debug | `Globals.h`, `DebugManager.*` | Shared runtime state, serial debug gate |
| OLED (dormant) | `OLED.*` | SSD1306 status line — written but not wired into the sketch |

### Configuration Storage

All settings live in a single JSON document at `/config.json` on SPIFFS, loaded at boot into a
`spiffs_config_t` struct that the components hold a pointer to. The file is written when the
settings menu times out, when the web UI posts a change, and on a factory reset. Running
statistics (`uptimeMins`, `tubeOnTimeMins`) live separately in `/stats.json` and are flushed once
per day, so a power cut loses at most a day of counting.

Holding the button while the board boots, or visiting `/factoryreset`, restores the defaults
compiled into `ClockDefs.h`, `DisplayDefs.h`, `LEDManager.h` and `NtpAsync.h`. A factory reset
also arms `testMode`, so the next boot runs the LED/tube test sequence before starting normally.

---

## Web Interface

Once on WiFi the clock serves a Bootstrap-based UI on port 80, reachable by IP or, via mDNS, at
`http://<hostname>.local`. HTTP Basic Authentication is **on by default** (`admin` / `setup`) and
covers everything except the root status page and the stylesheet.

| Path | Auth | Purpose |
|------|------|---------|
| `/` | no | Status page — network, NTP, clock, stats and ESP8266 information |
| `/time` | yes | NTP pool, POSIX TZ string, update interval |
| `/clockconfig` | yes | Every clock setting — display, blanking, LDR, backlights, indicators, auth |
| `/utility` | yes | Links to the maintenance actions below |
| `/setvalue` | yes | Show an arbitrary value on the tubes |
| `/ntpupdate` | yes | Force an immediate NTP sync |
| `/resetwifi` | yes | Clear stored WiFi credentials and restart |
| `/reset` | yes | Restart the module |
| `/factoryreset` | yes | Restore factory configuration, leaving WiFi credentials alone |
| `/debug` | yes | Turn serial debug output on |
| `/update` | `admin` / `update` | OTA firmware upload (`ESP8266HTTPUpdateServer`) |
| `/local.css` | no | Stylesheet, served from PROGMEM |

The status page reports current IP, MAC and SSID, NTP pool, TZ, last sync and time until the next
one, uptime, firmware version, LDR reading and resulting digit brightness, motion-sensor state,
active time source, RTC presence and time, impressions per second, lifetime clock-on and tube-on
hours, and the usual ESP sketch/heap/flash figures.

`/setvalue` takes the display over directly:

```
http://<clock-ip>/setvalue?value=123456&time=60&format=333333
```

`value` is shown modulo 10⁶, `time` is the display duration in seconds (255 = until replaced), and
each digit of `format` sets that tube's display type — `0` blanked, `1` normal, `2` blinking.

> **Note on defaults.** Both the web UI (`admin`/`setup`) and the OTA endpoint (`admin`/`update`)
> ship with fixed credentials; the OTA pair is hard-coded in `setup()`. Change the web password in
> the settings before putting the clock on a shared network.

---

## Operating the Clock

Full button, display-mode, settings-menu and web-UI documentation is in
**[`Software/README.md`](Software/README.md)**. In brief: the clock is driven by a **single
button** (GPIO0 / D3), so press length selects the action. A short press in TIME mode steps
through the temporary displays (date, LDR level, version, IP address, refresh rate, chip ID); a
1-second hold enters the 36-entry settings menu and steps through it, while short presses change
the value of the entry you are on. A short press while blanked suppresses blanking for 10 seconds,
an hour or four hours. Holding the button at power-on performs a factory reset.

Settings cover the display (hour format, leading zero, date format, pre-heat), blanking (day,
hour range, PIR, and what gets blanked), ambient dimming, the slots date effect, the backlight
and underlight strategies, and the separator and indicator LEDs. The menu exits and saves after
10 seconds without a press; a temporary display reverts to the time after 5 seconds.

---

## Hardware Summary

| Component | Details |
|-----------|---------|
| MCU | Wemos D1 Mini (ESP8266) @ 160 MHz |
| Display | 6 Numitron tubes, 7 segments + indicator each, 48 channels total |
| Drivers | 6× TLC5916 (DA-2000, IV-13 boards) or 3× DM13A (IV-9 board) |
| Backlights | WS2812B on GPIO3 (RX), driven by DMA |
| RTC | DS1307 on I2C — GPIO4 (D2) SDA, GPIO5 (D1) SCL |
| Ambient light | LDR on A0 |
| Motion | PIR on GPIO2 (D4), optional, software-selectable pull-up |
| Button | GPIO0 (D3), active low |
| Shift chain | DATA GPIO12 (D6), CLOCK GPIO13 (D7), LATCH GPIO14 (D5), BLANK/OE GPIO16 (D0, PWM) |

Three KiCad projects live under `Hardware/`, one per tube type; all share the same pinout and run
the same firmware unmodified. The DA-2000 and IV-9 designs include schematic and dimension PDFs.

---

## Documentation

| Document | Contents |
|----------|----------|
| [`Software/README.md`](Software/README.md) | User guide — WiFi setup, boot sequence, button operation, display modes, settings menu, web UI, backlights, blanking, pre-heat |
| [`Software/SPECIFICATION.md`](Software/SPECIFICATION.md) | Technical specification — GPIO, display pipeline and timings, mode state machine, config JSON schema, HTTP endpoints, source file reference |
| [`Hardware/da2000-6-digit/DA2000-Clock-Schematic.pdf`](Hardware/da2000-6-digit/DA2000-Clock-Schematic.pdf) | DA-2000 schematic (plus `-Dimensions.pdf`) |
| [`Hardware/iv9-6-digit/IV-9-6-Digit-Schematic.pdf`](Hardware/iv9-6-digit/IV-9-6-Digit-Schematic.pdf) | IV-9 schematic (plus `-Dimensions.pdf`) |

---

## Licence

Non-commercial. Personal, hobby, educational and not-for-profit use is granted
free of charge; selling hardware or software built from this, or using it in the
course of a business, needs a separate commercial licence. Derivative works must
stay under the same terms. See [LICENSE](LICENSE) for the full text and contact
details. Third-party libraries used by the firmware, and the Bootstrap and
jQuery assets the web pages load, remain under their own licences.

Copyright © 2018-2026 Ian Sparkes.

---

Firmware version **7** (`SOFTWARE_VERSION` in `Software/include/ClockDefs.h`).
