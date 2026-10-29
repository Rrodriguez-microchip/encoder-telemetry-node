# 8 · From Scratch

> The capstone. "If you were starting this project without me, how would you do
> it?" This doc is the order to build in, the proof-point at each stage, and the
> pitfalls to avoid — distilled from how this project actually went. It's the
> doc to re-read before you start the *next* embedded-plus-network project,
> whatever it measures.

---

## 8.1 The single most important habit: build in provable stages

The deepest lesson of this whole project isn't the quadrature table or the
W5500 — it's the **order**. We did not build the exciting part (the network)
first. We built:

```mermaid
flowchart LR
    p0["P0 bring-up<br/><i>chip runs,<br/>LED, UART</i>"] --> lcd["P2a LCD<br/><i>I can<br/>show text</i>"]
    lcd --> enc["P1 encoder<br/><i>the count<br/>is correct</i>"]
    enc --> tel["telemetry<br/><i>emits JSON<br/>over a cable</i>"]
    tel --> eth["P3 Ethernet<br/><i>the Pi<br/>can ping it</i>"]
    eth --> mqtt["P4 MQTT<br/><i>the broker<br/>sees the data</i>"]
    mqtt --> rob["P5 robustness<br/><i>survives a<br/>dead broker</i>"]

    classDef done fill:#e9f6ec,stroke:#4a9c5d,color:#143021;
    class p0,lcd,enc,tel,eth,mqtt,rob done;
```

Each phase **ended with a concrete, testable proof** before the next began. The
network — the part that feels like "the point" — came *last*, bolted onto a node
that already worked and could already tell you what it was doing over a serial
cable.

Why this order wins: when something breaks, **only the newest layer can be at
fault**, because every layer under it was already proven. When MQTT didn't
publish, we knew the encoder, the JSON, the W5500, and the ping all worked — so
the bug was in the ~100 lines of MQTT, not in 2,000 lines of unknown. Debugging a
system where only the top layer is unproven is a completely different experience
from debugging one where everything is suspect at once.

> **If you take one thing from this guide: never build the whole thing and then
> turn it on. Build the smallest piece that can prove *one* layer works, prove
> it, then add the next. The serial `printf` is your best friend — get text out
> of the chip on day one and you can see everything after that.**

---

## 8.2 The build order, with the proof point for each

Here's the sequence I'd follow again, and *what you're allowed to believe* once
each passes.

| Stage | Build | Proof it works | Now you can trust… |
|---|---|---|---|
| 1 | Clock + UART + LED | `printf("hello")` appears; LED blinks | the chip, clocks, toolchain, and your serial lifeline |
| 2 | 1 ms tick (timebase) | a counter printed once a second increments correctly | your sense of time — every later task depends on it |
| 3 | LCD | text on the panel | the display and your GPIO |
| 4 | Encoder *logic* on the PC | Unity tests pass (gcc, no board) | the math: counts, floor-div, RPM, debounce |
| 5 | Encoder *backend* on hardware | turn the knob, counts track on UART/LCD | the pin wiring and the ISR/poll path |
| 6 | Telemetry builder | correct JSON line on the UART | the exact bytes you'll later put on the network |
| 7 | W5500 alive | `VERSIONR == 0x04` | SPI wiring, clock, chip, reset |
| 8 | W5500 on the network | the Pi can `ping` the node | MAC/IP/subnet, the cable, the switch |
| 9 | MQTT | `mosquitto_sub` shows your JSON | the broker, topics, the whole stack |
| 10 | Robustness | survives broker-off-at-boot, runs for 24 h | it's a product, not a demo |

Notice stage 4 — **the pure logic is tested on your laptop before any hardware
is involved.** And stages 7–9 each isolate exactly one networking layer (doc 4),
so a failure points at one thing. This table *is* the project.

---

## 8.3 The networking build order, specifically

Since networking was the weak spot, here's that slice in close-up — the order
that keeps each failure diagnosable (all from [doc 4](04-ethernet-deep-dive.md)):

1. **Prove the chip over SPI first.** Read `VERSIONR`. Expect `0x04`. If you get
   `0x00`/`0xFF`, it's SPI/wiring/clock — stop, don't touch IP settings. This one
   byte saves hours.
2. **Give it an identity, read it back.** Set the static MAC/IP/subnet, then
   *read them back* and confirm they stuck before trusting them.
3. **Prove reachability with ping.** Lowest-level network test there is. If ping
   fails, it's addressing/subnet/cable — not your application. (This is where the
   `/32` vs `/24` subnet bug lived — doc 4.3.)
4. **Only now, TCP + MQTT.** With ping working, a failed MQTT connect is about
   the broker or your MQTT code, nothing below.

Each step rules out everything beneath it. That's the entire art of network
bring-up: **always have a test for the layer below the one you're working on, and
pass it first.**

---

## 8.4 The pitfalls, collected

The hard-won ones, so you can avoid them cold next time:

**Networking**
- **Subnet masks must be right and consistent.** A `/32` where you meant `/24`
  makes a device think it's alone on the network; ARP never leaves, ping is 100%
  loss, and it looks like a hardware fault. (doc 4.3)
- **A direct cable has no DHCP.** Two devices wired point-to-point must both get
  static IPs on the same subnet — nothing hands them out.
- **The W5500 has no MAC of its own.** You assign one; make it unique per node or
  two nodes on one LAN will fight intermittently. (doc 4.2)
- **Don't block on a network connect where a watchdog or real-time loop can't
  tolerate the stall.** Make it fast-failing and bounded, move it into your task
  loop, let it retry. This is what reset-looped the node. (doc 6.3)

**Embedded**
- **No `%f` on a soft-float MCU.** It drags in a huge library. Use fixed-point
  integers and split whole/fraction at print time. (doc 3.5)
- **Unsigned, wrap-safe time math:** `now - last >= PERIOD`, never
  `now >= last + PERIOD`, never `==`. A months-long uptime *will* hit the wrap.
  (doc 2.2)
- **ISRs set flags, nothing more.** No `printf`, no blocking, no real work in an
  interrupt. (doc 2.3)
- **Integer division truncates toward zero.** Negative counts need explicit
  floor division or you lose a unit around the origin. (doc 3.3)
- **Seed the quadrature `prev` state from the live pins** or the first edge
  registers a bogus count. (doc 3.2)

**Toolchain / project hygiene**
- **Vendored code is configured through build settings, never edited.** For the
  W5500: a `_WIZCHIP_=W5500` define + include paths, not a source edit — edits
  vanish on update. (doc 4.11)
- **Two separate MPLAB steps:** adding a `.c` to the build *and* adding its
  include directory. "No such file" = missing include dir; "undefined reference"
  = missing `.c` or missing `#define`. (doc 4.11)
- **Never hand-edit generated config.** Change it in MCC and regenerate. (doc 2.6)
- **A 5 V HD44780 LCD needs 5 V on VDD**, even when driven by 3.3 V logic — on
  3.3 V VDD it renders faintly and looks like a contrast fault. (cost an hour;
  it's in `CLAUDE.md`)

**Process**
- **`printf` on day one.** A serial lifeline out of the chip makes every later
  stage observable. Build it first.
- **Pull pure logic into host-testable files and actually write the tests.** The
  negative-count floor-division bug would never show up turning a knob forward;
  a unit test catches it in milliseconds. (doc 3.9)
- **Keep a decisions log and a gotchas list** (we used `CLAUDE.md`). Future-you,
  or the next engineer, will re-make every undocumented decision from scratch.

---

## 8.5 What you'd choose differently at larger scale

Part of understanding a design is knowing its edges — where *these* choices stop
being right:

- **More counts/second** (very high PPR or RPM, past ~50 k edges/s): the software
  quadrature decode runs out of CPU. Move to a dedicated counter chip (LS7366R)
  or a SAM E5x with the hardware PDEC peripheral. (doc 3.8)
- **Many nodes / real security:** the bench broker runs open (`allow_anonymous`).
  Production wants usernames, passwords, ACLs, and probably TLS on MQTT.
- **History and trends:** this shows live values only. Add InfluxDB + Grafana
  (subscribe to the same topics, store, chart) — no node changes needed, which is
  the pub/sub payoff again.
- **Config in the field:** IP, node ID, and `feet_per_rev` are compile-time
  placeholders headed for NVM + a UART CLI (phase P5). A deployed fleet needs
  per-node config that survives a reflash and doesn't require rebuilding firmware
  per machine.
- **A unique MAC from hardware:** read it from a small EEPROM (the wiring doc
  reserves one) instead of hardcoding, so every node is unique without a custom
  build.

---

## 8.6 The one-paragraph philosophy

Measure the smallest honest thing and prove it. Keep the hardware layer thin and
the logic layer testable. Let the specialised silicon (the W5500) do the hard,
generic work (TCP/IP) so your tiny CPU does only what's specific to you (MQTT and
the math). Decouple producers from consumers through a neutral middle (a broker,
a data object) so each side can change without the other. Assume every external
thing — the UART, the broker, the network — will misbehave, and contain each
failure so the core keeps running. Build in provable stages, lead with the
serial `printf`, write down why you chose what you chose. Do that, and the next
project — whatever it measures — goes the way this one did.

---

*End of the guide. Start back at the [README](README.md) for the index, or jump
to whichever doc you want to revisit. The code itself is commented in the same
spirit — read it alongside these docs.*
