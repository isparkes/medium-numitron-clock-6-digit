# ESP 6-Digit Numitron Clock Firmware — Technical Specification

Version: **7** (`SOFTWARE_VERSION`, `include/ClockDefs.h`)
Platform: **ESP8266 (Wemos D1 Mini)** via **PlatformIO / Arduino framework**

---

## 1. System Overview

A polled, single-threaded firmware for a six-digit Numitron clock. There is no display interrupt:
the main loop rebuilds and shifts out the complete 48-bit display frame on every pass, at roughly
100 Hz, and global brightness is handled by one hardware PWM channel driving the drivers' BLANK /
OE input. Time comes from NTP over asynchronous UDP, is mirrored into a DS1307 RTC, and is held
between the two by TimeLib. Configuration lives in a JSON file on SPIFFS and is editable from a
single button or from a web server that generates its pages in C++.

The same binary runs on all three board variants in `Hardware/`; they differ only in tube type and
in which constant-current driver fills the 48-channel chain.

---

## 2. Hardware Specification

### 2.1 Microcontroller

| Parameter | Value |
|-----------|-------|
| MCU | ESP8266 (Tensilica L106) |
| Board | Wemos D1 Mini |
| CPU frequency | 160 MHz (`board_build.f_cpu = 160000000L`) |
| Flash | 4 MB; SPIFFS used for configuration and statistics |

### 2.2 GPIO Assignment

| GPIO | D-pin | Direction | Function |
|------|-------|-----------|----------|
| GPIO0 | D3 | Input, pull-up | Button (active low) |
| GPIO2 | D4 | Input, pull configurable | PIR motion sensor |
| GPIO3 | RX | Output | NeoPixel data (WS2812B, DMA) |
| GPIO4 | D2 | I2C SDA | DS1307 RTC |
| GPIO5 | D1 | I2C SCL | DS1307 RTC |
| GPIO12 | D6 | Output | Shift register DATA |
| GPIO13 | D7 | Output | Shift register CLOCK |
| GPIO14 | D5 | Output | Shift register LATCH |
| GPIO16 | D0 | Output, PWM | Driver BLANK / OE — global brightness |
| A0 | A0 | Analogue input | LDR ambient light sensor |

Pin constants live in `OutputManagerNumi6.h` (display chain), `ClockDefs.h` (PIR, LDR),
`ClockButton.h` (button) and `LEDManager.h` (NeoPixel).

### 2.3 Display Subsystem

| Parameter | Value |
|-----------|-------|
| Digits | 6 |
| Bits per digit | 8 — segments a–g plus one indicator (`SEG_IND`) |
| Chain length | 48 bits |
| Drivers | 6× TLC5916 (DA-2000, IV-13) or 3× DM13A (IV-9) |
| Interface | Bit-banged synchronous serial, MSB first, latched on LATCH rising edge |
| Brightness | Hardware PWM on BLANK/OE, `analogWriteRange(1023)` |
| Refresh | One frame per main-loop pass, ~100 Hz, reported as *impressions per second* |

Segment and glyph tables are in `OutputManagerNumi6.h`: `SEG_A`…`SEG_G`, `SEG_IND`, `DIGIT_0`…
`DIGIT_9`, and a set of letter glyphs (`LETTER_S`, `LETTER_P`, `LETTER_n`, …) used for the boot
messages. A 16-entry `digitData[]` maps values 0–15 to patterns, so the display can render hex.

### 2.4 LED Subsystem

| Parameter | Value |
|-----------|-------|
| LED type | WS2812B |
| Pixels addressed | `DIGIT_COUNT * 2` = 12 — 6 backlight, 6 underlight |
| Boards populate | 6 |
| Data pin | GPIO3, `Neo800KbpsMethod` (DMA) |
| Colour order | `NeoGrbFeature` |
| Gamma | 256-entry `dim_curve[]` applied last |
| Fixed-colour quantisation | 16-entry `rgb_backlight_curve[]`, so channels are set 0–15 |

Separator and indicator LEDs are **not** NeoPixels. They hang off the 8th channel of the digit
drivers and are set through the `SEG_IND` bit of each digit byte.

### 2.5 Real-Time Clock

| Parameter | Value |
|-----------|-------|
| IC | DS1307 |
| Address | 0x68 |
| Interface | I2C, `Wire.begin(4, 5)` |
| Encoding | BCD, converted in `ESP_DS1307.cpp` |
| Battery backup | Yes (coin cell) |

The RTC is optional. `testRTCTimeProvider()` probes for it at boot; `useRTC` and `onceHadAnRTC`
record the result, so the clock can report that an RTC was present and has since stopped
responding.

### 2.6 Input Devices

**Button** (GPIO0, active low, internal pull-up) — `ClockButton`:

- Debounce: 5 consecutive polled reads at the debounce counter before the press is accepted.
- Durations measured from the moment debounce completes: > 1 s, > 2 s, > 8 s.
- Both *held* states (`isButtonPressedNS`) and *released-after* states
  (`isButtonPressedReleasedNS`) are exposed, which is what lets the UI preview an action while the
  button is down and commit it on release. The released flags are one-shot — reading clears them.

**LDR** (A0) — `getDimmingFromLDR()`:

```
raw      = 1023 - analogRead(A0)
if raw < sensorLDRMinValue            → return sensorLDRMinValue
adjusted = (raw - min) * sensorLDRSensitivity / 80 + min
if adjusted > sensorLDRMaxValue       → return sensorLDRMaxValue
else ramp sensorLDRSmoothed ±1 toward adjusted and return it
```

The ±1-per-loop ramp is the smoothing: at ~100 Hz a full-scale change takes about ten seconds, so
the display never steps visibly. `sensorSmoothCountLDR` is persisted but unused — it is a leftover
from the sibling Nixie firmware's sample-count smoothing. When `useLDR` is false the fixed value
`setDim` is returned instead.

**PIR** (GPIO2) — `checkPIR()`:

- Pull-up is software-selectable (`usePIRPullup`), since modules differ in output drive.
- The pin is sampled on every loop pass and the highs are counted; the count is compared once a
  second against half the measured impressions per second, which makes the threshold adapt to the
  actual loop rate rather than assuming one.
- The first time the sensor pulls the pin low, `pirInstalled` latches true and PIR blanking takes
  priority over all day/hour rules from then on.
- Timeout: 60–3600 s, default 300 s.

---

## 3. Software Architecture

### 3.1 Module Overview

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| Main sketch | `ESP8266Clock.ino` | `setup()`, `loop()`, mode state machine, periodic processing, all web page handlers |
| Output manager | `OutputManagerNumi6.cpp/.h` | Singleton display buffer, glyph tables, blank/blink/smooth/pre-heat, 48-bit shift out, PWM brightness |
| LED manager | `LEDManager.cpp/.h` | NeoPixel colour strategies, brightness pipeline, gamma, boot diagnostic LEDs |
| NTP component | `NtpAsync.cpp/.h` | Async UDP NTP client, POSIX TZ, scheduling, millis-overflow handling |
| RTC component | `ESP_DS1307.cpp/.h` | DS1307 register access, BCD conversion |
| Config storage | `SPIFFS.cpp/.h` | `/config.json` and `/stats.json` read/write |
| HTML server | `HtmlServer.cpp/.h` | Page scaffolding, form widgets, argument parsing, auth accessors |
| Transition engine | `DA2000-Transition.cpp/.h` | Slots effects — wipe in/out, bang in/out |
| Button handler | `ClockButton.cpp/.h` | Debounce FSM, press-duration detection |
| Utilities | `ClockUtils.cpp/.h` | IP/time formatting, string and BCD helpers |
| Globals | `Globals.h` | Shared runtime state, component instantiation |
| Debug manager | `DebugManager.cpp/.h` | Serial debug gate (115200 baud) |
| OLED (dormant) | `OLED.cpp/.h` | SSD1306 status line — implemented, not included by the sketch |

Components are plain singletons declared `static` in their headers (`ntpAsync`, `spiffs`,
`ledManager`, `oled`, `debugManager`, `Clock`); `OutputManager` uses a private constructor with a
`getInstance()` accessor and an `outputManager` reference. Cross-component notification is by
function-pointer callback — `DebugCallback` for log lines, `NewTimeCallback` for "NTP delivered a
new time".

### 3.2 Execution Model

```
setup()
  │
  ├─ OutputManager::setUp()      — pins, analogWriteRange(1023), BLANK low
  ├─ NtpAsync::setUp()           — setenv("TZ"), tzset, register time callback
  ├─ DebugManager / callbacks    — wire debug output into NTP and SPIFFS
  ├─ LEDManager::setUp()         — NeoPixelBus Begin()
  ├─ Wire.begin(4, 5)            — I2C up early, so NTP can seed the RTC at once
  ├─ button held now?            — factoryReset() + ESP.restart()
  ├─ SPIFFS::getConfigFromSpiffs()
  │    └─ no config → factoryReset(); no SPIFFS → red diagnostic LED
  ├─ [testMode] tube/LED test loop, exits on button press at digit 8
  ├─ WiFiManager::autoConnect()  — saved SSID → AP portal "SetMeUp!" (60 s)
  ├─ NTP: immediate query, then retry each second for up to 10 s
  ├─ setServerPageHandlers(); server.begin()
  ├─ httpUpdater.setup(&server, "/update", "admin", "update")
  ├─ mdns.begin(hostname); addService("http", "tcp", 80)
  ├─ setPIRPullup(); testRTCTimeProvider(); startRTCTimeProvider()
  └─ show version; load statistics; set day-of-week

  Each stage paints a word on the tubes and one backlight LED:
  green = ok, yellow = in progress, red = failed, blue = reset performed.

loop()  [delay(10) at the foot — ~100 Hz]
  │
  ├─ server.handleClient(); mdns.update()
  ├─ impressionsPerSec++
  ├─ [second changed] performOncePerSecondProcessing()
  │      ├─ latch impressions/sec, decrement temp-mode and blank-suppression timers
  │      ├─ settings-menu timeout → leave menu and save config
  │      ├─ evaluate PIR and day/hour blanking → setTubesAndLEDSblankMode()
  │      ├─ decrement the /setvalue display counter (0xff = forever)
  │      ├─ reset PIR debounce counter
  │      ├─ NTP due? → getTimeFromNTP()
  │      └─ update indicator LED states
  ├─ [:00]    performOncePerMinuteProcessing()  — re-read RTC, stats, IP, select transition
  ├─ [hh:00]  performOncePerHourProcessing()    — reconnectDroppedConnection()
  ├─ [00:00]  performOncePerDayProcessing()     — flush stats, set day-of-week
  ├─ button1.checkButton()
  ├─ preview next mode on 1 s / 2 s hold; commit on release
  ├─ nextMode != currentMode ? setNextMode() : processCurrentMode()
  ├─ sample PIR pin, read and smooth LDR, push to LED and output managers
  ├─ outputManager.outputDisplay()   — build and shift the frame
  └─ setLeds()                       — separators, indicators, NeoPixels
```

`performOncePerSecondProcessing()` is driven by a change in `second()`, not by elapsed millis, so
it stays aligned with the displayed time. The minute/hour/day calls are nested inside the
`second() == 0` branch and guarded by `triggeredThisSec` so they fire exactly once.

### 3.3 Display Mode State Machine

`currentMode` and `nextMode` are both in play: holding the button advances `nextMode` and
`setNextMode()` renders a preview of it; releasing copies `nextMode` into `currentMode` and
`processCurrentMode()` takes over, where a short press edits the value.

| Range | Modes | Notes |
|-------|-------|-------|
| 0 | `MODE_TIME` | Normal running; temporary displays and slots live here |
| 1–6 | `MODE_HOURS_SET` … `MODE_YEARS_SET` | Skipped when `ntpTimeValid()` |
| 7–34 | Configuration entries | See §8.2 for the backing fields |
| 35 | `MODE_VERSION` | Read-only |
| 36 | `MODE_TUBE_TEST` | `MODE_MAX`; wraps back to 1 |

**Conditional skipping** is done by `setNewNextMode(n)`, which sets `nextMode = n` and
`currentMode = n - 1` so the state machine lands on the wanted entry on the next pass:

| Condition | Jumps to |
|-----------|----------|
| NTP time valid, at `MODE_HOURS_SET` | `MODE_12_24` |
| Day blanking is Never/Weekend/Weekday/Always, at `MODE_HR_BLNK_START` | `MODE_USE_LDR` |
| Backlight mode is not Fixed, at `MODE_RED_CNL` | `MODE_CYCLE_SPEED` |
| Backlight mode is not Cycle, at `MODE_CYCLE_SPEED` | `MODE_BL_DIM` |

**Rendering of configuration entries** — `loadNumberArrayConfInt(value, modeNumber)` puts the
value in digits 0–3 and the mode number in digits 4–5; `displayConfig()` then marks digits 4 and 5
`BLINK`. So a settings screen always reads `VVVV MM` with a blinking mode number.

**Temporary display modes** (`TEMP_MODE_*`, 0–6) are a separate axis inside `MODE_TIME`, selected
by short presses and timed out by `tempDisplayModeDuration`. They force the display un-blanked
while active.

---

## 4. Time Management

### 4.1 Time sources

TimeLib's internal clock is the one the display reads. It is seeded and corrected from, in order
of authority:

1. **NTP** — on success, `setTimeFromServer()` sets TimeLib and `setRTCTime()` writes the DS1307.
2. **DS1307** — read into TimeLib once per minute, which is more accurate than letting the ESP
   free-run between NTP syncs.
3. **Manual** — settings entries 1–6, available only while NTP time is not valid.

The status page reports which of these is in force.

### 4.2 NTP

| Parameter | Value |
|-----------|-------|
| Transport | `ESPAsyncUDP`, port 123, 48-byte packet |
| Pool | `pool.ntp.org` by default |
| Update interval | 60–86 400 s, default **7 261 s** |
| Epoch conversion | `secsSince1900 - 2208988800` |
| Validity window | `(now - lastUpdate) < 2000 × interval` ms, i.e. twice the interval |

`getTimeFromNTP()` resolves the pool with `WiFi.hostByName()`, sends the request, and registers an
`onPacket` lambda; the response path converts to `time_t`, formats
`YYYY,MM,DD,HH,MM,SS` through `localtime()` and fires the new-time callback. Scheduling is checked
once per second via `getNextUpdate()`, which returns negative when a sync is due and always
returns `-1` before the first successful sync.

`checkMillisOverflow()` resets the schedule if `millis()` has wrapped (every ~49.7 days), so an
uptime rollover cannot park the clock in a state where the next sync never comes due.

### 4.3 Timezone handling

The POSIX TZ string is applied with `setenv("TZ", tzs, 1)` and `tzset()` at setup, and conversion
is done by the C library's `localtime()`. Default:
`CET-1CEST,M3.5.0,M10.5.0/3`. DST is therefore handled by the rule in the string, with no code
of its own.

---

## 5. Display Rendering

`OutputManager::outputDisplay()` runs once per loop pass. Inputs are the digit buffer
(`numberArray[6]` values and `displayType[6]` modes) and the current LDR reading.

### 5.1 Pipeline

```
digit buffer (values + per-digit display type)
  → blankDigits()   — BLANKED, or BLINK in its off phase, becomes DIGIT_BLANK
  → smoothDigits()  — a changed digit is OR'd with its previous pattern for _tubeLag ms
  → shiftDigitsOut()
       ├─ OR in separator LEDs   (SEG_IND on digits 1–4, from led1State/led2State)
       ├─ OR in indicator LEDs   (SEG_IND on digits 0 and 5, from ledLState/ledRState)
       ├─ optional alternate-frame dimming of those LEDs
       ├─ pre-heat: every nth frame, OR 0x7f into all six digits
       └─ shiftOut48()
  → setBlankingPin() — analogWrite(BLANK, _lastLDRValue)
```

### 5.2 Blink

Blink is used for the settings-mode digit pair and for `/setvalue` format `2`.

| Constant | Value | Meaning |
|----------|-------|---------|
| `BLINK_COUNT_ON` | 70 | Frames lit |
| `BLINK_COUNT_OFF` | 55 | Frames dark |

The asymmetry is deliberate: a Numitron filament lights slowly, so an even duty cycle reads as a
dim flicker rather than a blink.

### 5.3 Smoothing (filament cross-fade)

```
_tubeLag = (0.000655 × b² − 0.01159 × b + 148) / (6 − preheatStrength)      [ms]
                                 where b is the LDR brightness value
```

While a digit is within `_tubeLag` of changing, the old and new segment patterns are displayed
together, so the filament cross-fades. The lag is recomputed only when the LDR value changes.
Smoothing is suppressed for a digit returning from `BLANKED`, where a cross-fade from nothing
would look wrong, tracked per digit by `digitWasBlanked[]`.

`smoothDigits()` also raises `ledManager.setSyncColourTime()` while any digit is in transition,
which is what keeps Colour-Time backlights in step with the digits.

### 5.4 Pre-heat

| Constant | Value | Effect |
|----------|-------|--------|
| `PREHEAT_V_STRONG` | 1 | All segments lit on every other frame |
| `PREHEAT_STRONG` | 2 | …every third frame |
| `PREHEAT_WEAK` | 3 | …every fourth frame (**default**) |
| `PREHEAT_V_WEAK` | 4 | …every fifth frame |
| `PREHEAT_OFF` | 5 | Disabled |

A counter increments per frame; when it passes `preheatStrength` it resets and `0x7f7f7f` is OR'd
into both halves of the frame, lighting all seven segments of all six digits for that frame only.
Keeping filaments warm shortens the rise time of a segment that is about to be needed. Note the
same setting divides `_tubeLag` (§5.3), so stronger pre-heat also shortens the cross-fade.

### 5.5 Brightness

Brightness is global, not per digit: `analogWrite()` on the BLANK/OE pin at a range of 1023.

```
_ldrValue      = 1023 - ldrBrightness      // BLANK is active high: higher duty = dimmer
_lastLDRValue  = _ldrValue                 // while not blanked
_lastLDRValue++ (to 1023)                  // while blanked — a slow fade to black
```

Because blanking ramps the duty by one step per frame, the display fades out over up to ~10 s at
100 Hz rather than snapping off.

### 5.6 Bit packing

```
out2 = digit[2] | (digit[1] << 8) | (digit[0] << 16)     // shifted first
out1 = digit[5] | (digit[4] << 8) | (digit[3] << 16)     // shifted second
```

`shiftOut48()` clocks out `out2` then `out1`, 24 bits each, MSB first, then raises LATCH. Digit 0
therefore leaves the ESP first. Within a digit byte the order is
`SEG_IND, SEG_G, SEG_F, SEG_E, SEG_D, SEG_C, SEG_B, SEG_A` from MSB to LSB.

`outputDisplayDiags()` packs and latches the raw `numberArray` without the blank/smooth/pre-heat
stages, which is how the boot messages put letter glyphs on the tubes.

### 5.7 Slots transitions

Three `Transition` objects are constructed in `Globals.h`:

| Object | In | Out | Hold | Effect |
|--------|-----|-----|------|--------|
| `transitionWipe` | 800 ms | 700 ms | 2800 ms | `SLOTS_MODE_WIPE_WIPE` |
| `transitionBang` | 400 ms | 400 ms | 3200 ms | `SLOTS_MODE_BANG_BANG` |
| `transitionDummy` | 0 | 0 | 0 | Null-object guard |

`activeTransition` points at one of them, re-selected once per minute and on entry to TIME mode.
The transition captures both the date (target) and the time (regular) displays at `start()`, then
drives digits between them; `updateRegularDisplaySeconds()` keeps the seconds live underneath so
the time is correct when the effect ends. Trigger frequency is `slotsFreq` — off, or every 1, 5
or 10 minutes.

---

## 6. LED Backlight

### 6.1 Colour strategies

| Strategy | Constant | Description |
|----------|----------|-------------|
| Fixed | `BACKLIGHT_FIXED` | One colour from `redCnl`/`grnCnl`/`bluCnl`, each 0–15, expanded through `rgb_backlight_curve[]` |
| Cycle | `BACKLIGHT_CYCLE` | Random walk: pick a random channel and a random number of steps (≤ 256), raise that channel and lower the other two one step at a time; `cycleSpeed` (4–64) sets how many frames pass between steps |
| Colour-Time | `BACKLIGHT_COLOUR_TIME` | Per-digit colour from `colourTimeR/G/B[10]`, indexed by the value that digit is showing |
| Day-of-Week | `BACKLIGHT_DAY_OF_WEEK` | One colour from `dayOfWeekR/G/B[7]`, updated at midnight |

The Colour-Time palette is documented in `LEDManager.h` as a deliberately adjusted set — the pure
HSV values are commented out below it because 2/3 and 6/7 were not distinguishable in practice.

### 6.2 Brightness pipeline

```
raw channel value
  → × _pwmFactor      (if useBLPulse — a 0…1 ramp derived from the position within the second)
  → × _backlightDim   (backlightDimFactor / 100, or extDimFactor / 100 for underlights)
  → × _ldrDimFactor   (if useBLDim — ldrValue / 1023)
  → dim_curve[]       (perceptual gamma, 256 entries)
  → NeoPixelBus buffer → Show()
```

Backlight and underlight run the same pipeline with separate dim factors
(`getLEDAdjustedBL()` / `getLEDAdjustedUL()`). When blanked, both are driven to zero through the
same path, so the dim curve's floor is respected.

The pulse factor alternates direction each second (`upOrDown` in `setLeds()`), giving a triangular
once-per-second breathing envelope rather than a sawtooth.

### 6.3 Separator and indicator LEDs

These are driven through the display chain, not the NeoPixels. `setLeds()` computes `led1State`
and `led2State` from `ledMode`:

| Constant | Behaviour |
|----------|-----------|
| `LED_RAILROAD` | The two separators alternate |
| `LED_BLINK_SLOW` | Both blink, slow |
| `LED_BLINK_FAST` | Both blink, fast |
| `LED_BLINK_DBL` | Double blink per second |
| `LED_ON` | Always on |
| `LED_OFF` | Always off |

`ledLState` and `ledRState` are the two outer indicators, each independently assigned a status
source by `processStatusMode()`:

| Constant | Lit when |
|----------|----------|
| `STATUS_LED_MODE_NTP` | NTP time is currently valid |
| `STATUS_LED_MODE_ONLINE` | WiFi is connected |
| `STATUS_LED_MODE_AM_PM` | `isPM()` |
| `STATUS_LED_MODE_PIR` | The PIR pin reads high |
| `STATUS_LED_MODE_OFF` / `_ON` | Never / always |

Defaults are Online (left) and NTP valid (right). `separatorDimFactor` halves their brightness by
lighting them on alternate frames; `doNotDimIndLEDs` exempts the two status indicators from that.

### 6.4 Boot diagnostic LEDs

`setDiagnosticLED(stage, state)` lights backlight pixel `stage` in red, yellow, green or blue and
leaves pixels beyond it at a dim 0x1f, so startup progress fills the display from the left. Each
call in the sketch is followed by a 500 ms delay to make the stages readable. Stages are
`DIAGS_START`, `DIAGS_SPIFFS`, `DIAGS_WIFI`, `DIAGS_NTP`, `DIAGS_RTC`, `DIAGS_DEBUG` (0–5), with
`AP_MODE` (6) reserved for the portal message rather than a stage.

---

## 7. Blanking System

### 7.1 Day strategies (`DAY_BLANKING_*`)

| Constant | Blank when |
|----------|------------|
| `DAY_BLANKING_NEVER` | Never (default) |
| `DAY_BLANKING_WEEKEND` | `weekday()` is 1 or 7 |
| `DAY_BLANKING_WEEKDAY` | `weekday()` is 2–6 |
| `DAY_BLANKING_ALWAYS` | Always |
| `DAY_BLANKING_HOURS` | Within the configured hour range |
| `DAY_BLANKING_WEEKEND_OR_HOURS` | Weekend **or** hour range |
| `DAY_BLANKING_WEEKDAY_OR_HOURS` | Weekday **or** hour range |
| `DAY_BLANKING_WEEKEND_AND_HOURS` | Weekend **and** hour range |
| `DAY_BLANKING_WEEKDAY_AND_HOURS` | Weekday **and** hour range |

`getHoursBlanked()` handles a range that crosses midnight (`start > end` → OR of the two
segments); `start == end` disables hour blanking.

### 7.2 Blanking target (`BLANK_MODE_*`)

| Constant | Tubes | LEDs |
|----------|-------|------|
| `BLANK_MODE_TUBES` | blanked | lit |
| `BLANK_MODE_LEDS` | lit | blanked |
| `BLANK_MODE_BOTH` | blanked | blanked (default) |

### 7.3 Precedence

- Blanking is evaluated **only** in `MODE_TIME`; any other mode forces `blanked = false`.
- If a PIR has been detected, `checkBlanking()` returns false unconditionally and motion decides.
  This is intentional: a PIR and a day/hour rule would otherwise fight.
- Blanking suppression (`blankSuppressedMillis`) overrides everything. A short press while blanked
  steps through 10 s, 1 hour and 4 hours; the step selector itself times out after 5 s
  (`blankSuppressedSelectionTimoutMillis`).

---

## 8. Configuration Storage

### 8.1 File system

SPIFFS, mounted on demand — each access does `SPIFFS.begin()` … `SPIFFS.end()` rather than holding
the mount open.

| File | Contents |
|------|----------|
| `/config.json` | All configuration, one flat object |
| `/stats.json` | `uptimeMins`, `tubeOnTimeMins` |

Parsing is ArduinoJson **v5** (`DynamicJsonBuffer`, `JsonObject&`, `parseObject`). The v5 API is
not source-compatible with v6, which is why the dependency is pinned exactly.

### 8.2 JSON schema

| Key | Type | Range / values | Default | Menu # |
|-----|------|----------------|---------|--------|
| `ntp_pool` | string | hostname | `pool.ntp.org` | web |
| `ntp_update_interval` | int | 60–86400 s | 7261 | web |
| `time_zone_string` | string | POSIX TZ | `CET-1CEST,M3.5.0,M10.5.0/3` | web |
| `hourMode` | bool | false = 24 h | false | 7 |
| `blankLeading` | bool | true = blank leading zero | false | 8 |
| `dateFormat` | byte | 0 YYMMDD, 1 MMDDYY, 2 DDMMYY | 2 | 9 |
| `dayBlanking` | byte | 0–8 | 0 | 10 |
| `blankHourStart` | byte | 0–23 | 0 | 11 |
| `blankHourEnd` | byte | 0–23 | 7 | 12 |
| `blankMode` | byte | 0 tubes, 1 LEDs, 2 both | 2 | 13 |
| `useLDR` | bool | — | true | 14 |
| `sensorLDRMinValue` | int | 100–600 | 450 | 15 |
| `sensorLDRMaxValue` | int | 600–1023 | 1023 | 16 |
| `sensorLDRSensitivity` | int | 50–200 % | 100 | web |
| `setDim` | int | 0–1023, used when `useLDR` is false | 1023 | web |
| `slotsMode` | byte | 0 wipe, 1 bang | 0 | 17 |
| `slotsFreq` | byte | 0 off, 1/2/3 = 1/5/10 min | 1 (see §8.3) | 18 |
| `ledMode` | byte | 0–5 separator pattern | 0 | 19 |
| `pirTimeout` | int | 60–3600 s | 300 | 20, 21 |
| `usePIRPullup` | bool | — | true | 33 |
| `backlightMode` | byte | 0 fixed, 1 cycle, 2 colour-time, 3 day-of-week | 1 | 22 |
| `redCnl` / `grnCnl` / `bluCnl` | byte | 0–15 each | 15 / 0 / 0 | 23–25 |
| `cycleSpeed` | byte | 4–64 | 10 | 26 |
| `backlightDimFactor` | byte | 10–100 % | 100 | 27 |
| `extDimFactor` | byte | 10–100 % | 100 | 28 |
| `useBLDim` | bool | LDR dims the backlights | true | web |
| `useBLPulse` | bool | once-per-second pulse | false | web |
| `separatorDimFactor` | byte | 1 bright, 2 dim | 2 | 29 |
| `statusModeL` / `statusModeR` | byte | 0–5 | Online / NTP | 30, 31 |
| `doNotDimIndLEDs` | bool | — | false | 32 |
| `preheatStrength` | byte | 1–5 | 3 | 34 |
| `webAuthentication` | bool | — | true | web |
| `webUsername` | string | — | `admin` | web |
| `webPassword` | string | — | `setup` | web |
| `testMode` | bool | run the test pattern at next boot | true after reset | — |
| `scrollback` | bool | **unused** — vestigial | — | — |
| `antiGhost` | byte | **unused** — vestigial | — | — |
| `sensorSmoothCountLDR` | int | **unused** — see §2.6 | 500 | — |

### 8.3 Persistence behaviour

- Loaded once at boot into `current_config`; components hold a pointer to it, so a change takes
  effect on the next frame without a reload.
- Button-menu changes are written to RAM immediately and flushed to flash when the menu times out
  after 10 s, so holding through several entries costs one write.
- Web changes are written immediately, but only if something actually changed —
  `saveToSpiffsIfChanged()` exists to avoid needless flash wear.
- Statistics are written once a day; a power cut loses at most a day of counting.
- `factoryReset()` rewrites every field from the compiled-in defaults and saves. Note that it does
  **not** reset `slotsFreq`, which keeps whatever value the struct already held (0 — off — on a
  board whose config has never loaded).
- A factory reset sets `testMode = true`, so the next boot runs the test pattern; the sketch clears
  the flag and saves once the pattern is dismissed with a button press.

---

## 9. WiFi & Networking

### 9.1 Connection sequence

```
Boot
  ├─ WiFiManager::autoConnect(hostname, "SetMeUp!")
  │    ├─ saved credentials → connect → WiFi.setAutoReconnect(true)
  │    └─ no credentials or failure → AP configuration portal (60 s timeout)
  │         └─ configModeCallback() shows "AP  On" and the portal IP on the tubes
  └─ no connection → run offline on RTC time, myIP = softAPIP()
```

`reconnectDroppedConnection()` runs once an hour as a backstop to `setAutoReconnect`.

### 9.2 Web server

`ESP8266WebServer` on port 80, pages generated in C++ from the helpers in `HtmlServer.cpp`
(`getHTMLHead`, `getNavBar`, `getFormHead`, `getDropDownHeader`, `getNumberInput`, …). There is no
filesystem image: the only static asset is the stylesheet, served from PROGMEM at `/local.css`.

| Endpoint | Auth | Handler | Purpose |
|----------|------|---------|---------|
| `/` | no | `rootPageHandler` | Status page |
| `/time` | yes | `timeServerPageHandler` | NTP pool, TZ, interval |
| `/clockconfig` | yes | `clockConfigPageHandler` | All clock settings |
| `/setvalue` | yes | `setDisplayValuePageHandler` | Arbitrary display value |
| `/utility` | yes | `utilityPageHandler` | Maintenance links |
| `/ntpupdate` | yes | `ntpUpdatePageHandler` | Force a sync, 302 to `/utility` |
| `/resetwifi` | yes | `resetWiFiPageHandler` | Clear credentials, restart |
| `/reset` | yes | `resetPageHandler` | Restart, 302 with a 5 s refresh to `/` |
| `/factoryreset` | yes | `factoryResetPageHandler` | Defaults, keep WiFi |
| `/debug` | yes | `debugPageHandler` | Enable serial debug, 302 to `/utility` |
| `/local.css` | no | `localCSSHandler` | Stylesheet from PROGMEM |
| `/update` | `admin`/`update` | `ESP8266HTTPUpdateServer` | OTA firmware upload |

Authentication is applied per route by a lambda wrapper that calls `server.authenticate()` and
returns `requestAuthentication()` on failure; the credentials come from
`webUsername` / `webPassword`, defaulting to `admin` / `setup`. The OTA credentials are a separate,
hard-coded pair passed to `httpUpdater.setup()` in `setup()`.

mDNS is registered as the ESP hostname with an `http`/`tcp` service on port 80, and `mdns.update()`
is pumped from the main loop.

**`/setvalue` parameters**

| Parameter | Meaning |
|-----------|---------|
| `value` | Value to display, taken `% 1000000` (required) |
| `time` | Seconds to display, clamped to 255 = indefinite |
| `format` | Six digits, one per tube: 0 = blanked, 1 = normal, 2 = blinking |

---

## 10. Numitron-Specific Behaviour

Where the sibling Nixie firmware has anti-cathode-poisoning, this firmware has the inverse set of
concerns — a filament display has no cathode poisoning, but it is slow and it is driven at constant
current:

| Feature | Rationale |
|---------|-----------|
| Pre-heat (§5.4) | Keeps unlit filaments warm so they rise quickly |
| Smoothing (§5.3) | Turns the slow fall time into a deliberate cross-fade |
| Asymmetric blink (§5.2) | Compensates for unequal rise and fall |
| PWM on BLANK/OE (§5.5) | Constant-current drivers dim by duty cycle, not by current |
| Slow blanking fade (§5.5) | One PWM step per frame, so the display dies away rather than cutting |
| Tube-on hour counter | Filament life is the consumable; tracked separately from uptime |

There is no ACP mode, no fade/scroll transition engine and no multiplexing — all six digits are
driven continuously, which is why the frame is simply rewritten each pass.

---

## 11. Build Configuration

### 11.1 `platformio.ini`

```ini
[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200
monitor_port = /dev/cu.usbserial-110
monitor_rts = 0
monitor_dtr = 0
board_build.f_cpu = 160000000L
```

`monitor_rts = 0` and `monitor_dtr = 0` stop the serial monitor resetting the board when it
attaches. No `upload_port` or `upload_speed` is set, so PlatformIO's auto-detection applies.

### 11.2 Compile-time configuration

There is no `Configuration.h` and no feature-flag system — unlike the Nixie firmware, every
feature here is compiled in and controlled at runtime. The compile-time constants that matter are:

| Constant | Where | Value |
|----------|-------|-------|
| `SOFTWARE_VERSION` | `ClockDefs.h` | 7 |
| `DIGIT_COUNT` | `ClockDefs.h`, `OutputManagerNumi6.h` | 6 |
| `CONFIG_PORTAL_TIMEOUT` | `ClockDefs.h` | 60 s |
| `RTC_I2C_ADDRESS` | `ClockDefs.h` | 0x68 |
| `LED_DOUT` | `LEDManager.h` | GPIO3 |
| `HTML_TITLE` | `HtmlServer.h` | `ESP 6-Digit Numitron Clock` |
| `WEB_*_DEFAULT` | `HtmlServer.h` | `admin` / `setup` / auth on |

### 11.3 Debug output

`DebugManager::setUp(bool)` opens `Serial` at 115200. The flag is a local `debugVal` in `setup()`,
false as shipped; `/debug` turns it on at runtime until the next restart. `NtpAsync` and
`SPIFFS_CLOCK` route their messages through the same gate by callback, prefixing their own tags.

---

## 12. Key Timing Constants

| Constant | Value | Description |
|----------|-------|-------------|
| Main loop delay | 10 ms | Sets the nominal ~100 Hz frame and PIR sample rate |
| `debounceCounter` | 5 reads | Button debounce, ~50 ms at the loop rate |
| Button thresholds | 1000 / 2000 / 8000 ms | Long, longer and very long presses |
| `TEMP_DISPLAY_MODE_DUR_MS` | 5 000 ms | Temporary display timeout |
| `TIME_SET_MODE_DUR_MS` | 10 000 ms | Settings menu timeout and auto-save |
| `BLINK_COUNT_ON` / `_OFF` | 70 / 55 frames | Asymmetric blink duty |
| Boot stage dwell | 500 ms | Literal `delay(500)` in `setDiagnosticLED()`; the matching `DIAGS_DELAY_TIME` constant is defined but unused |
| `CONFIG_PORTAL_TIMEOUT` | 60 s | WiFiManager portal |
| NTP boot retry window | 10 s | One query per second at startup |
| `NTP_UPDATE_INTERVAL_DEFAULT` | 7 261 s | Default sync interval (prime-ish, to avoid beating) |
| NTP validity | 2 × interval | `ntpTimeValid()` window |
| `PIR_TIMEOUT_DEFAULT` | 300 s | Motion timeout |
| `SENSOR_SMOOTH_READINGS_DEFAULT` | 500 | Persisted, unused |

---

## 13. Source File Reference

| File | Lines | Key symbols |
|------|-------|-------------|
| [src/ESP8266Clock.ino](src/ESP8266Clock.ino) | 2753 | `setup()`, `loop()`, `performOncePerSecondProcessing()`, `setNextMode()`, `processCurrentMode()`, `checkBlanking()`, `checkPIR()`, `getDimmingFromLDR()`, `factoryReset()`, all page handlers |
| [src/OutputManagerNumi6.cpp](src/OutputManagerNumi6.cpp) | 693 | `OutputManager`, `outputDisplay()`, `shiftDigitsOut()`, `shiftOut48()`, `blankDigits()`, `smoothDigits()`, `setBlankingPin()`, the `loadNumberArray*()` family |
| [src/SPIFFS.cpp](src/SPIFFS.cpp) | 384 | `getConfigFromSpiffs()`, `saveConfigToSpiffs()`, `getStatsFromSpiffs()`, `saveStatsToSpiffs()` |
| [src/HtmlServer.cpp](src/HtmlServer.cpp) | 353 | HTML scaffolding and form widgets, `checkServerArg*()`, auth accessors |
| [src/LEDManager.cpp](src/LEDManager.cpp) | 306 | `LEDManager`, `processLedStatus()`, `cycleColours3()`, `getLEDAdjustedBL/UL()`, `setDiagnosticLED()` |
| [src/NtpAsync.cpp](src/NtpAsync.cpp) | 250 | `NtpAsync`, `getTimeFromNTP()`, `ntpTimeValid()`, `getNextUpdate()`, `checkMillisOverflow()` |
| [src/ClockButton.cpp](src/ClockButton.cpp) | 189 | `ClockButton`, `checkButtonInternal()`, press/release predicates |
| [src/OLED.cpp](src/OLED.cpp) | 162 | `OLED` status line — implemented, not linked in |
| [src/DA2000-Transition.cpp](src/DA2000-Transition.cpp) | 149 | `Transition`, `wipeInWipeOut()`, `bangInBangOut()` |
| [src/ClockUtils.cpp](src/ClockUtils.cpp) | 122 | `formatIPAsString()`, `secsToReadableString()`, `timeToReadableString()`, BCD and hex helpers |
| [src/ESP_DS1307.cpp](src/ESP_DS1307.cpp) | 106 | `DS1307`, `getTime()`, `setTime()`, `startClock()`, BCD conversion |
| [src/DebugManager.cpp](src/DebugManager.cpp) | 27 | `DebugManager::setUp()`, `debugMsg()` |
