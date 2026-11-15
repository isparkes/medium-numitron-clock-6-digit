# ESP 6-Digit Numitron Clock — Firmware

Firmware for a six-digit Numitron clock based on the **Wemos D1 Mini (ESP8266)**. WiFi setup via a
captive portal, NTP time with a DS1307 RTC as backup, WS2812B backlighting, ambient-light dimming,
a motion sensor, and a full configuration menu reachable from a single button or from the built-in
web interface.

This is the user guide. For the architecture, timings, JSON schema and source-file reference see
**[SPECIFICATION.md](SPECIFICATION.md)**; for the project as a whole see the
[top-level README](../README.md).

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU | Wemos D1 Mini (ESP8266) @ 160 MHz |
| Display | 6 Numitron tubes — 7 segments + 1 indicator per digit |
| Drivers | 6× TLC5916 (DA-2000, IV-13 boards) or 3× DM13A (IV-9 board), 48 channels either way |
| Backlights | WS2812B on GPIO3 |
| RTC | DS1307 on I2C (GPIO4 / GPIO5) |
| Light sensor | LDR on A0 |
| Motion sensor | PIR on GPIO2 (optional) |
| Button | 1× push button on GPIO0 |

### Pin Mapping

| Signal | GPIO | D-pin |
|--------|------|-------|
| Button | GPIO0 | D3 |
| PIR input | GPIO2 | D4 |
| NeoPixel DATA | GPIO3 | RX |
| RTC SDA | GPIO4 | D2 |
| RTC SCL | GPIO5 | D1 |
| Shift register DATA | GPIO12 | D6 |
| Shift register CLOCK | GPIO13 | D7 |
| Shift register LATCH | GPIO14 | D5 |
| Driver BLANK / OE (PWM) | GPIO16 | D0 |
| LDR input | A0 | A0 |

Note that LATCH and BLANK are **not** on the same pins as the sibling Nixie clock — the Numitron
boards put LATCH on D5 and use D0 as a PWM brightness control.

---

## Building & Flashing

This project uses [PlatformIO](https://platformio.org/).

```bash
# Build
pio run -e d1_mini

# Upload
pio run -e d1_mini -t upload

# Serial monitor at 115200 baud
pio run -e d1_mini -t monitor
```

There is no filesystem image to upload — every web page is generated in C++ at request time, and
SPIFFS holds only the `config.json` and `stats.json` files the clock writes itself. The monitor
port in `platformio.ini` is `/dev/cu.usbserial-110`; change it for your own adapter. RTS and DTR
are held off so that opening the monitor does not reset the board.

Serial debug output is off by default. Turn it on at runtime by visiting `/debug` in the web
interface; it stays on until the next restart.

---

## First Boot

The clock narrates its own startup, both as words on the tubes and as a row of backlight LEDs
filling in from the left — one LED per stage, **green** for success, **yellow** for in progress,
**red** for failure, **blue** when a reset was performed.

| Tubes show | Stage |
|------------|-------|
| `StArt_` | Core startup |
| `SPIFFS` | Configuration file system mounted and config loaded |
| `nEt UP` | WiFi connection |
| `ntP UP` | First NTP sync (up to 10 s of retries) |
| `rtC UP` | DS1307 detection and time seeding |
| `ALL On` | Startup complete |
| `AP  On` | Fell back to the configuration portal |

### WiFi setup

1. On first boot the clock tries to reconnect to any previously saved network.
2. If that fails it opens a **WiFi configuration portal** as an open access point named after the
   ESP hostname, with the password `SetMeUp!`. The tubes show `AP  On`.
3. Connect a phone or laptop to that AP and enter your network credentials in the portal.
4. The portal times out after 60 seconds, after which the clock runs offline on RTC time.

Once connected, the clock syncs from NTP, starts its web server and registers an mDNS name, so it
can be reached at `http://<hostname>.local` as well as by IP. Short-press the button a few times
to read the IP address off the tubes (temporary modes 3 and 4).

If no RTC is fitted and no network is available, the clock still runs — it just starts from an
arbitrary time, which you can set by hand in the settings menu.

---

## Button Operation

Everything is done with **one button**, so the length of the press selects the action.

| Action | Effect |
|--------|--------|
| Short press in TIME mode | Step through the temporary display modes |
| Short press inside a setting | Change that setting's value |
| Short press while blanked | Suppress blanking — 1 press = 10 s, 2 presses = 1 hour, 3 presses = 4 hours |
| 1-second hold | Step to the next settings-menu entry; from TIME mode this enters the menu |
| Held down at power-on | Factory reset, then restart |

The action is previewed while you hold the button and committed when you release it, so you can
back out of a long press by continuing to hold until the display shows what you want.

Timeouts: a temporary display reverts to the time after **5 seconds**; the settings menu exits and
saves after **10 seconds** without a press.

---

## Display Modes

### Normal (TIME) mode

Shows `HH MM SS`, in 12- or 24-hour format, with or without the leading zero blanked. The
separator "colon" LEDs between the digit pairs follow the separator LED mode (setting 19), and the
two outer indicator LEDs show whatever status you assign them (settings 30 and 31).

Once a minute, 5 minutes or 10 minutes — your choice — the display runs the **slots** effect,
wiping or banging the date in, holding it, and taking it back out again.

### Temporary display modes

Short-press the button in TIME mode to step through these:

| # | Shows |
|---|-------|
| 0 | Date, in the configured format |
| 1 | Smoothed LDR reading in the left four digits (higher = brighter) |
| 2 | Firmware version |
| 3 | IP address, octets 1–2 |
| 4 | IP address, octets 3–4 |
| 5 | Impressions per second — the display refresh rate |
| 6 | ESP chip ID used for the mDNS name (skipped when offline) |

Each one reverts to the time after 5 seconds. They also un-blank the display while showing.

### Settings menu

Hold the button for a second to enter the menu, and again for each following entry. A short press
changes the value of the entry you are on.

While in the menu the display reads **`VVVV MM`** — the value in the left four digits, and the
blinking two right-hand digits telling you which numbered entry you are on. The numbers below are
what those two digits show.

Entries 1–6 set the time and date and are **skipped automatically whenever NTP time is valid** —
there is no point setting by hand what the network will overwrite. Entries 11–12 are skipped
unless the day-blanking mode involves hours, 23–25 unless the backlight mode is Fixed, and 26
unless it is Cycle.

| # | Setting | Values |
|---|---------|--------|
| 1 | Hours set | Increment |
| 2 | Minutes set | Increment |
| 3 | Seconds set | Press resets to 00 |
| 4 | Days set | Increment |
| 5 | Months set | Increment |
| 6 | Years set | Increment |
| 7 | Hour format | 0 = 24 h, 1 = 12 h |
| 8 | Blank leading zero | 0 = show, 1 = blank |
| 9 | Date format | 0 = YY MM DD, 1 = MM DD YY, 2 = DD MM YY |
| 10 | Day blanking | 0 Never, 1 Weekend, 2 Weekday, 3 Always, 4 Hours, 5 Weekend-or-Hours, 6 Weekday-or-Hours, 7 Weekend-and-Hours, 8 Weekday-and-Hours |
| 11 | Blanking start hour | 0–23 |
| 12 | Blanking end hour | 0–23 |
| 13 | Blank mode | 0 Tubes, 1 LEDs, 2 Both |
| 14 | Use LDR | 0 = off (fixed brightness), 1 = on |
| 15 | LDR dark point | 100–600, steps of 10 |
| 16 | LDR bright point | 600–1023, steps of 10 |
| 17 | Slots effect | 0 = wipe in/out, 1 = bang in/out |
| 18 | Slots frequency | 0 Off, 1 every minute, 2 every 5 min, 3 every 10 min |
| 19 | Separator LED mode | 0 Railroad, 1 Slow blink, 2 Fast blink, 3 Double blink, 4 On, 5 Off |
| 20 | PIR timeout up | 60–3600 s, steps of 10 |
| 21 | PIR timeout down | 60–3600 s, steps of 10 |
| 22 | Backlight mode | 0 Fixed, 1 Cycle, 2 Colour-Time, 3 Day-of-Week |
| 23 | Red channel | 0–15 |
| 24 | Green channel | 0–15 |
| 25 | Blue channel | 0–15 |
| 26 | Cycle speed | 4–64 |
| 27 | Backlight dim factor | 10–100 %, steps of 5 |
| 28 | Underlight dim factor | 10–100 %, steps of 5 |
| 29 | Separator dim | 1 = bright, 2 = dim |
| 30 | Left indicator mode | 0 NTP valid, 1 Online, 2 AM/PM, 3 PIR, 4 Off, 5 On |
| 31 | Right indicator mode | as above |
| 32 | Do not dim indicators | 0 = dim with the separators, 1 = keep bright |
| 33 | PIR pull-up | 0 = off, 1 = on |
| 34 | Pre-heat strength | 1 very strong … 4 very weak, 5 off |
| 35 | Version | Read-only |
| 36 | Tube test | All segments, cycling |

Because the menu wraps, holding the button repeatedly from entry 36 brings you back to 1.

---

## Web Interface

With WiFi connected, browse to `http://<clock-ip>/` or `http://<hostname>.local/`. The pages are
Bootstrap-based and work from a phone.

HTTP Basic Authentication is **on by default**, with the username `admin` and password `setup`.
Everything except the status page and the stylesheet asks for it. Change the password on the clock
configuration page before putting the clock on a shared network.

| Page | What it does |
|------|--------------|
| `/` | Status — network, NTP, clock, statistics and ESP8266 information |
| `/time` | NTP pool, POSIX timezone string, update interval |
| `/clockconfig` | Every clock setting, including the ones not in the button menu |
| `/utility` | Links to the maintenance actions |
| `/setvalue` | Put an arbitrary value on the tubes |
| `/update` | OTA firmware upload — separate credentials, `admin` / `update` |

The utility page offers: clear the WiFi configuration and restart, restart the module, update the
firmware, force an NTP sync now, and factory reset without touching the WiFi credentials.

The status page is the quickest way to check the clock's health: it reports the NTP pool and
timezone in use, when the last sync happened and when the next is due, the current LDR reading and
the resulting digit brightness, the motion sensor state, which time source is in use, whether the
RTC is present and what it thinks the time is, impressions per second, and the lifetime
clock-on and tube-on hours.

A few settings exist only on the web page and not in the button menu — the LDR sensitivity
percentage, the fixed brightness used when the LDR is off, the backlight pulse and backlight-dim
switches, and the web authentication credentials.

### Showing your own value

```
http://<clock-ip>/setvalue?value=123456&time=60&format=333333
```

| Parameter | Meaning |
|-----------|---------|
| `value` | The number to show, taken modulo 1 000 000 |
| `time` | How long to show it, in seconds; 255 means until something replaces it |
| `format` | One digit per tube — `0` blanked, `1` normal, `2` blinking |

Only `value` is required. This takes over the display from the normal time readout, so it is a
convenient way to use the clock as a readout for something else on your network.

---

## Backlight Modes

| Mode | Behaviour |
|------|-----------|
| Fixed | One colour, set by the red, green and blue channel settings (0–15 each) |
| Cycle | Slow random walk through the colour space, at the configured cycle speed |
| Colour-Time | Each digit takes a colour matching the value it is showing (0–9) |
| Day-of-Week | One colour for the whole display, changing with the day |

The firmware addresses twelve pixels — six backlights and six underlights — on a single data line;
the current boards populate six. Backlight and underlight brightness are scaled separately
(settings 27 and 28), and both are scaled again by the LDR when backlight dimming is enabled and
by an optional once-per-second pulse. A perceptual brightness curve is applied last, so low
settings stay smooth rather than collapsing to off.

In Colour-Time mode the colours re-sync whenever a digit changes, so the display changes colour in
step with the time rather than drifting.

---

## Blanking

The display can be blanked to save the tubes. Three things can trigger it:

- **Day of week** — weekends, weekdays or always.
- **Hour range** — between a start and an end hour. The range may cross midnight; setting start
  and end to the same hour disables it.
- **Motion** — if a PIR is fitted, blank after the configured timeout with no motion.

Day and hour rules can be combined with AND or OR (settings 10). **If a PIR is detected, it takes
over**: the day and hour rules are ignored and motion alone decides. The clock works out for
itself whether a PIR is fitted, the first time the sensor pulls the pin low.

What gets blanked — tubes, LEDs or both — is setting 13. Blanking only ever applies in TIME mode,
so you can always get into the menu. A short press while blanked suppresses blanking for 10
seconds, an hour, or four hours, depending on how many times you press.

---

## Pre-heat and Smoothing

Numitron filaments are incandescent, so they light and fade slowly. Two features work with that
rather than against it:

- **Pre-heat** (setting 34) briefly lights every segment on a regular fraction of frames, keeping
  unlit filaments warm so they come up faster when they are needed. Stronger pre-heat means a
  faster response and a slightly visible glow on "off" segments; `5` turns it off.
- **Smoothing** cross-fades a changing digit by lighting the old and new segment patterns together
  for a few milliseconds. The duration is derived from the ambient light reading — longer when the
  display is dim — and shortened as pre-heat gets stronger. It is deliberately suppressed when a
  digit comes back from blanked, where a cross-fade would look wrong.

The blink used in the settings menu is asymmetric for the same reason: 70 frames on against 55
off, so a blinking digit reads as blinking rather than as merely flickering.

---

## Factory Reset

Either hold the button down while powering the clock on, or use **Factory reset** on the utility
page. Both restore the compiled-in defaults and save them.

The button-at-power-on route also restarts the clock. The web route leaves the WiFi credentials
alone; to clear those as well, use **Clear WiFi configuration and restart**.

After a factory reset the clock arms its test mode, so the next startup runs an LED and tube test
pattern. Press the button when all six digits show `8` to end the test and carry on booting
normally — the clock then clears the test flag so it does not run again.

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| paulstoffregen/Time | ^1.6.1 | Timekeeping and time arithmetic |
| me-no-dev/ESPAsyncUDP | 0.0.0-alpha (pinned SHA) | Async NTP UDP client |
| bblanchon/ArduinoJson | 5.13.4 | JSON config persistence (v5 API) |
| makuna/NeoPixelBus | ^2.7.5 | WS2812B LED control |
| tzapu/WiFiManager | ^0.16.0 | WiFi AP configuration portal |
| adafruit/Adafruit GFX Library | ^1.11.5 | Graphics primitives for the optional OLED |
| adafruit/Adafruit SSD1306 | ^2.5.7 | Optional status OLED |

`OLED.cpp` / `OLED.h` implement an SSD1306 status display but are not currently included by the
main sketch; the two Adafruit libraries are listed so that the file still compiles.

---

## Firmware Version

Current: **7** (`SOFTWARE_VERSION` in `include/ClockDefs.h`).
