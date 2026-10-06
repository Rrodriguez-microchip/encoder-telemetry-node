# CLAUDE.md — Encoder Telemetry Node

Working context for AI assistants (Claude Code CLI and Claude Cowork) on this repo.
**Read this whole file before doing anything.** Background and rationale are in
`PROJECT_OVERVIEW.md`; wiring is in `Encoder Telemetry Node Wiring & Pin Map.md`;
MCC setup is in `SAMD21 Node MCC Harmony 3 Setup Walkthrough.md`.

---

## 1. Current state  ← update this section at the end of every session

- **Phase:** P0 ✅ · P2a LCD ✅ · timebase ✅ (tested) · **P1 encoder ✅ (tested on KY-040)** ·
  telemetry ✅ · robustness nice-to-haves (WDT, non-blocking log, config, speed_ft_s) ✅ (tested).
  **Next: Unity host tests, then P3 Ethernet.**
- **Last session (2026-10-06):** wrote the P1 encoder (3-layer backend-agnostic design, committed
  `4646546`), then telemetry + 4 robustness pieces (uncommitted, see below). All compile via the
  command-line build (§4) and were flashed + verified on hardware by Ramon.
- **Uncommitted (ready to commit — tested):** `services/{config,log,wdt,telemetry}.*`, `app/app.c`,
  plus MCC-generated WDT changes (`config/default/initialization.c` fuses, `plib_clock.c` GCLK2→WDT,
  and the project/MCC config files). All added to the MPLAB project.
- **Next steps:**
  1. **Unity host tests** (tomorrow): quadrature table, floor-div, RPM-timeout, button debounce —
     the §7 acceptance tests, run on the PC (gcc + Unity in `SAMD21_Project/tests/`). Pure-logic only.
  2. Then **P3 Ethernet**: W5500 bring-up, ioLibrary_Driver as a git submodule, VERSIONR==0x04, Pi ping.
- **Open MCC to-dos (Ramon):** none outstanding — GCLK2 is now repurposed to clock the WDT.
- **Hardware notes:** KY-040 is mechanically bouncy — `invalid` counter climbs and counts drift
  slightly; this is the cheap knob, not the firmware (the production optical encoder won't do it).
  RPM reads cleanly, which is what matters. LCD contrast best near the end of the pot's travel;
  backlight very bright — consider a 330–470 Ω series resistor on pin 15.

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
        ├── drivers/    lcd_hd44780.c/.h, encoder.h (API), encoder_core.c/.h (pure logic),
        │               encoder_poll.c (KY-040, in build), encoder_eic.c (optical, out of build)
        │               (later: w5500_port.c)
        ├── services/   delay.c/.h, timebase.c/.h, telemetry.c/.h, log.c/.h, config.c/.h,
        │               wdt.c/.h  (later: net_mqtt, cli)
        ├── config/default/   MCC-GENERATED, DO NOT EDIT
        └── packs/            device pack copy (committed)
```

**Encoder backends:** `encoder_poll.c` and `encoder_eic.c` both implement `encoder.h` — only
ONE may be in the MPLAB build at a time (same symbols). `encoder_poll.c` (1 ms polling) is the
KY-040 bench build; `encoder_eic.c` (interrupt) is for the production optical encoder.

Include path already has `../src`, so includes look like `"services/timebase.h"`.
Build (MPLAB X project `SAMD21_Project/Tristan_Excrusion_speed.X`, XC32 5.10,
DFP SAMD21 3.7.262, ARM CMSIS 6.3.0). Assistants compile-check from the command line
(Ramon does not need to run this himself):
```
cd SAMD21_Project/Tristan_Excrusion_speed.X
export PATH="/c/Program Files/Microchip/MPLABX/v6.30/gnuBins/GnuWin32/bin:/c/Program Files/Microchip/xc32/v5.10/bin:$PATH"
make -f Makefile CONF=default           # incremental;  add 'clean' first to force a full rebuild
```
Output: `dist/default/production/*.hex` / `*.elf`. A single file can be checked with `xc32-gcc`
+ the project include flags and `-mdfp="…/SAMD21_DFP/3.7.262/samd21d"` (quote the path — spaces).

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
GCLK2 = OSCULP32K / 31 ≈ 1.06 kHz → WDT (5-bit divider caps at 31, so ÷31 not ÷32; close enough).
TC3 = 1 ms tick (CC0 = 47999, MPWM). NVIC priorities: EIC 1, TC3 2. SysTick = free-running,
no interrupt, used only by `delay_us/ms`. TC4 is reserved for P1b.
**WDT:** enabled by config fuse (`initialization.c`: WDT_ENABLE, WDT_PER=CYC2048 ≈ 1.9 s, window
off). No WDT PLIB in this CSP — `services/wdt.c` kicks it with a direct CLEAR-key register write.

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
| Encoder is a 3-layer split: `encoder.h` API / `encoder_core` pure logic / per-sensor backend | Swapping KY-040 → optical encoder is one new backend file; API, core, telemetry, app unchanged. |
| Two backends: `encoder_poll.c` (knob) and `encoder_eic.c` (optical), one in the build at a time | Mechanical KY-040 bounces for ms; 1 ms polling beats edge interrupts for it. Optical needs interrupts (16.8k edges/s). |
| Core recovers a skipped quadrature state as ±2 in the last direction (not drop) | Dropping a both-bits-changed transition loses counts and drifts. Shaft really moved 2; only a reversal exactly on a skip errs, and self-corrects. |
| rpm as integer ×10, speed as milli-ft/s; conversion in telemetry not encoder | No %f (XC32 float lib). `ft_s = rpm/60 × feet_per_rev`; feet_per_rev lives in `config` (→ NVM in P5), not the sensor driver. |
| UART logging via `services/log.c` (non-blocking, drops), not raw printf | Generated `SERCOM5_USART_Write` spins forever on DRE; a wedged TX would hang the superloop. log_line drops the line instead. |
| WDT via config fuse + register kick (no PLIB); GCLK2 repurposed to clock it | This CSP exposes WDT only through fuses. A WDT reset is a normal reset; SWD reflash is never blocked by it. |
| Ramon configures peripherals/clocks in MCC himself, even when code could | Keeps generated config the single source of truth; matches the "never hand-edit config/default" rule. |

## 7. Phase plan & P1 spec

P0 bring-up ✅ · P2a LCD ✅ · timebase ✅ · **P1 encoder ✅** (+ telemetry + WDT/log/config done
early) · P1b TC4/EVSYS capture (optional, skip unless low-speed RPM is too jittery) · P2 encoder
values on the LCD ✅ (bench readout already live) · **P3 W5500** (ioLibrary_Driver as a git
submodule, VERSIONR == 0x04, ping) · P4 MQTT (`bldg/<area>/<node_id>/telemetry` QoS 0, `/status`
retained with last-will `"offline"`) · P5 robustness (reconnect, NVM config, UART CLI; WDT already
done) · P6 24 h soak test.

**Still owed before/around P3:** Unity host tests (below) — not a gate, but do them before the
network layers pile on. P1b and a nicer P2 layout are optional.

**P1 acceptance tests (verified on KY-040, UART):** 20 detents one way → count +80, det +20,
rev +1 · 20 back → count 0 · count at rest a multiple of 4 · one press = one event · RPM reads 0
within ~1 s of stopping. Note: the mechanical knob drifts a little (bounce → `invalid` climbs);
RPM is clean. The pure-logic versions of these become the Unity tests.

## 8. Current telemetry & next task (Unity tests)

**Telemetry payload now emitted ~10 Hz over UART (via non-blocking `log_line`):**
```
{"node":"node01","count":36,"det":9,"rev":0,"dir":0,"rpm":0.0,"ft_s":0.000,"btn":0}
```
`node`/`area`/`feet_per_rev` come from `services/config.c` (compile-time now, NVM in P5).
`ft_s = rpm/60 × feet_per_rev` (feet_per_rev default 1.000 — placeholder, calibrate on the machine).

**Next session — Unity host tests** (`SAMD21_Project/tests/`, gcc on the PC, no hardware):
test the pure `encoder_core` against the §7 acceptance list — forward/back count & floor-div
(incl. negatives), the ±2 invalid-recovery, RPM→0 after the stop timeout, one-press-one-event
debounce. `encoder_core.c/.h` are hardware-free by design so they link against Unity directly.
Needs a small host makefile; `tests/` is not part of the MPLAB build.

---

## Maintaining this file

At the end of a work session (or before switching tools), update:
- **§1 Current state**: phase, what was done, what's uncommitted, next steps, open to-dos. Rewrite; don't append.
- **§6 Decisions log**: add a row for any new non-obvious decision (what + why, one line).
- **§4 / §5**: only if the layout, pins, clocks or MCC settings changed.
Keep this file under about 250 lines; move long rationale into PROJECT_OVERVIEW.md.
