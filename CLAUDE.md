# CLAUDE.md — Encoder Telemetry Node

Working context for AI assistants (Claude Code CLI and Claude Cowork) on this repo.
**Read this whole file before doing anything.** Background and rationale are in
`PROJECT_OVERVIEW.md`; wiring is in `Encoder Telemetry Node Wiring & Pin Map.md`;
MCC setup is in `SAMD21 Node MCC Harmony 3 Setup Walkthrough.md`.

---

## 1. Current state  ← update this section at the end of every session

- **Phase:** P0 done. LCD driver done early (P2a). **Timebase written, NOT yet hardware-tested.**
- **Last session (2026-10-06):** wrote `services/timebase.c/.h` (TC3 1 ms tick, `timebase_ms()`,
  `timebase_us()` with an overflow-race guard) and turned `app.c` into a non-blocking superloop
  (LED 500 ms, tick line 500 ms, LCD uptime 1 s). Repo created and pushed to GitHub.
- **Uncommitted:** `services/timebase.*`, `app/app.c`. Ramon commits them after the hardware test.
- **Next steps:**
  1. Ramon: add `timebase.c` to the MPLAB project, build, flash, verify (see §8), commit.
  2. P1 encoder: propose the file plan (§7) again, wait for OK, then write the code.
- **Open MCC to-dos (Ramon):** disable the unused GCLK2 (DFLL/24 = 2 MHz, nothing uses it).
- **Hardware notes:** LCD contrast is best near the end of the pot's travel (normal for 5 V);
  the backlight is very bright, so consider a 330–470 Ω series resistor on pin 15.

---

## 2. Who / how to work

- User: **Ramon**, Field Application Engineer at Microchip. Give the direct answer first,
  then the *why*. Flag the pitfalls a senior engineer would catch.
- **Propose before writing big code; wait for OK.** Small, reviewable steps; one feature per commit.
- Ramon flashes and tests on the hardware. Never claim something works on hardware without his confirmation.
- Ramon adds new source files to the MPLAB project himself (Add Existing Item).
  **Do not edit `nbproject/configurations.xml`.**
- Commits: Ramon usually commits and pushes from Sourcetree. If an assistant commits, it's only
  with his OK, with a clear message, and never with untested firmware unless he says so.
- When switching tools (Cowork ⇄ Claude Code): finish, commit, and update §1 first, so two
  assistants never edit the same working tree with different context.

## 3. Hard rules (firmware)

- **Never edit anything under `SAMD21_Project/src/config/default/`** (MCC-generated). If generated
  code must change, tell Ramon which MCC setting to change.
- `main.c` stays minimal: `SYS_Initialize(NULL); app_run();`
- Bare-metal superloop with non-blocking tasks driven by the TC3 1 ms tick. No RTOS.
  Scheduling pattern: `if (now - t >= PERIOD) { t += PERIOD; ... }`. Unsigned subtraction,
  `>=`, never `==`, never `now >= t + PERIOD`. Use `t = now` only where catching up makes no sense.
- **ISRs only update state or set flags.** Never printf from an ISR.
- No `%f` in printf/snprintf (XC32 pulls in a large float library). Use fixed-point integers.
- Rate-limited UART logging. Telemetry: one JSON line at about 10 Hz, same shape as the future MQTT payload:
  `{"count":1234,"det":308,"rev":15,"dir":1,"rpm":42.5,"btn":0}`
- Encoder API (`drivers/encoder.h`) is backend-agnostic, with nothing ISR-specific exposed:
  `encoder_init, encoder_task, encoder_get_count, _detents, _revolutions, _rpm, _direction, _button`.
- Pure logic (state table, RPM math, debounce) lives in hardware-free files so it can be
  unit-tested on the host (gcc + Unity in `SAMD21_Project/tests/`).

## 4. Repo layout (actual)

```
/ (repo root = C:\Projects\Tristan's project)
├── CLAUDE.md, PROJECT_OVERVIEW.md, reference docs (.md)
└── SAMD21_Project/
    ├── Tristan_Excrusion_speed.X/        MPLAB X project (+ MCC config in *_default/)
    └── src/
        ├── main.c
        ├── app/        app.c/.h          top-level superloop
        ├── drivers/    lcd_hd44780.c/.h  (next: encoder.h, encoder_core.c/.h, encoder_ky040.c; later w5500_port.c)
        ├── services/   delay.c/.h, timebase.c/.h  (next: telemetry, log; later net_mqtt, config, cli)
        ├── config/default/   MCC-GENERATED, DO NOT EDIT
        └── packs/            device pack copy (committed)
```

Include path already has `../src`, so includes look like `"services/timebase.h"`.
Build: MPLAB X project `SAMD21_Project/Tristan_Excrusion_speed.X`, XC32 5.10, DFP SAMD21 3.6.144.

## 5. Hardware & pin map

SAM D21 Curiosity Nano (ATSAMD21G17D: 128 KB Flash / 16 KB RAM, 3.3 V, **NOT 5 V tolerant**)
on the Curiosity Nano Base for Click boards.

| Signal | Pin | Notes |
|---|---|---|
| ENC_A | PB02 | EIC EXTINT2, both edges, filter, interrupt; event output on (future TC4 capture) |
| ENC_B | PA04 | EIC EXTINT4, both edges, filter, interrupt |
| ENC_SW | PB03 | GPIO in, pull-up, polled |
| LCD_RS / LCD_E | PB08 / PB23 | GPIO out |
| LCD_D4..D7 | PA28 / PA05 / PA23 / PA03 | GPIO out; 1602A HD44780 at 5 V, 4-bit, RW tied to GND |
| ETH MOSI/SCK/MISO | PA16 / PA17 / PA19 | SERCOM1 SPI mode 0, 2 MHz, blocking (W5500 ETH WIZ Click, socket 1, **unplugged until P3**) |
| ETH_CS / RST / INT | PA18 / PA07 / PA06 | CS idle high |
| DBG TX/RX | PA22 / PB22 | SERCOM5 → Curiosity Virtual COM, 115200 8N1, blocking mode |
| LED0 | PB10 | active low |

KY-040 proof-of-concept encoder: 20 detents/rev, 4 counts/detent, 80 counts/rev, 3.3 V, has its own pull-ups.

**Clocks / interrupts:** DFLL48M → GCLK0 48 MHz (NVM RWS = 1). GCLK3 = DFLL/48 = 1 MHz → EIC.
TC3 = 1 ms tick (CC0 = 47999, MPWM). NVIC priorities: EIC 1, TC3 2. SysTick = free-running,
no interrupt, used only by `delay_us/ms`. TC4 is reserved for P1b.

## 6. Decisions log (don't re-litigate without a reason)

| Decision | Why |
|---|---|
| EIC on 1 MHz (GCLK3), not 32 kHz | 32 kHz caps edges at about 5 kHz (75 RPM at 1000 PPR). Bounce is handled by the quadrature state table, not the filter. |
| SERCOM5 stays in blocking mode | Debug-only UART at a limited rate; fine. Revisit only if loop timing suffers. |
| TC3 stays at 1 ms; µs from TC3's counter | A faster tick only adds interrupt load. `timebase_us()` = ms × 1000 + TC3 count / 48. |
| `dir` = 0 when stopped | A dashboard showing a direction on a stopped shaft is misleading. |
| SysTick for blocking delays | Doesn't touch TC3; used only for short LCD timing. |
| LCD updates via `lcd_write_line()` (overwrite + pad) | No clear → no flicker, no 2 ms wait. |
| Floor division for detents/revs | C truncates toward zero, so `-1/80 == 0` would be wrong. |
| RPM: edge timestamps (µs) in a ring buffer, averaged; 1 s with no edge → 0 | Smooth at low speed; µs resolution needed at 600 PPR (edges about every 60 µs). |
| Production target: 600 PPR at about 420 RPM (about 16.8 k edges/s) | Fits EIC + ISR (about 7% CPU). The SAMD21 has no hardware quadrature decoder; above about 50 k edges/s use an LS7366R or a SAME5x with PDEC. |
| FEET_PER_REV is calibrated on the machine, stored in NVM (P5) | The chain drives an unknown downstream geometry; the sprocket formula is only an estimate. |
| Repo outside OneDrive; GitHub private (Rrodriguez-microchip/encoder-telemetry-node) | OneDrive and `.git` conflict. |

## 7. Phase plan & P1 spec

P0 bring-up ✅ · P2a LCD ✅ · **timebase (in test)** · P1 encoder · P1b TC4/EVSYS capture
(optional) · P2 encoder values on the LCD · P3 W5500 (ioLibrary_Driver as a git submodule,
VERSIONR == 0x04, ping) · P4 MQTT (`bldg/<area>/<node_id>/telemetry` QoS 0, `/status` retained
with last-will `"offline"`) · P5 robustness (reconnect, WDT, NVM config, UART CLI) · P6 24 h soak test.

**P1 proposed files:** `drivers/encoder.h` (API), `drivers/encoder_core.c/.h` (pure: 16-entry
quadrature table with invalid-transition counter, floor-div helpers, RPM estimator, button
debouncer at 1 ms with 20 ms stability and a press-event counter), `drivers/encoder_ky040.c`
(EIC callbacks read both pins → core; critical-section snapshot), `services/telemetry.c/.h`
(10 Hz JSON), `services/log.c/.h`, plus `tests/` (Unity: table, RPM, button).

**P1 acceptance tests (UART):** 20 detents one way → count +80, det +20, rev +1 · 20 back →
count exactly 0 · count at rest is always a multiple of 4 · fast back-and-forth ending on the
start detent → start value · one press = exactly one event · RPM reads 0 within about 1 s of stopping.

## 8. Timebase test (pending)

Expect: LCD row 0 `up hh:mm:ss` matching a stopwatch; row 1 `tick N` every 500 ms; UART
`tick N  ms=…  dus=500000` (±a few µs; values near 499000 or jumps mean the overflow-race
guard is broken); LED toggling every 500 ms.

---

## Maintaining this file

At the end of a work session (or before switching tools), update:
- **§1 Current state**: phase, what was done, what's uncommitted, next steps, open to-dos. Rewrite; don't append.
- **§6 Decisions log**: add a row for any new non-obvious decision (what + why, one line).
- **§4 / §5**: only if the layout, pins, clocks or MCC settings changed.
Keep this file under about 250 lines; move long rationale into PROJECT_OVERVIEW.md.
