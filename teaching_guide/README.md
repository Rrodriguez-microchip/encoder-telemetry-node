# Teaching Guide — Encoder Telemetry Node

> A "turn the project into a class" walkthrough. The goal isn't to hand you
> working code — you already have that — it's to explain **why** every piece is
> built the way it is, so you could rebuild it (or defend it to a customer)
> without me. Heavy emphasis on the networking stack, since that's the part
> worth the most study.

This is a companion to the three reference docs at the repo root
(`PROJECT_OVERVIEW.md`, the wiring doc, the MCC setup walkthrough) and the
living context file (`CLAUDE.md`). Those tell you *what* the project is and its
current state. **This guide tells you why the code looks the way it does.**

---

## How to read this

The docs build on each other — read them in order the first time. After that,
each stands alone as a reference.

> **A note on the diagrams:** the inline diagrams in these docs are written in
> [Mermaid](https://mermaid.js.org/). They render automatically on GitHub and in
> VS Code's Markdown preview (`Ctrl+Shift+V`). In a plain-text viewer you'll see
> the diagram source in a fenced ```` ```mermaid ```` block instead — still
> readable, just not drawn.

| # | Doc | What you'll get out of it |
|---|-----|---------------------------|
| 1 | [The big picture](01-big-picture.md) | The whole signal path end to end, and why each technology was chosen over the alternatives. Start here. |
| 2 | [Firmware architecture](02-firmware-architecture.md) | The bare-metal superloop, the 1 ms tick, non-blocking tasks, the 3-layer encoder design. The skeleton everything else hangs on. |
| 3 | [The encoder & the math](03-encoder-and-math.md) | Quadrature decoding, the state table, floor division, RPM from timestamps, and why there's no floating point anywhere. |
| 4 | **[Ethernet deep dive](04-ethernet-deep-dive.md)** | **The big one.** What Ethernet and TCP/IP actually are, from the wire up. Why the W5500 does it in hardware. SPI transport, connection stages, IP/subnet math, and every W5500/ioLibrary file. |
| 5 | **[MQTT deep dive](05-mqtt-deep-dive.md)** | What MQTT is and why it beats HTTP here. Broker, publish/subscribe, topics, QoS, retained messages, last-will presence, keepalive, and the connect handshake. |
| 6 | [Robustness](06-robustness.md) | Watchdog, non-blocking logging, and the real war story of how the connect timing nearly bricked the node in a reset loop. |
| 7 | [The Python dashboard](07-the-python-dashboard.md) | The other end of the wire: the subscriber, paho's threading model, and how the GUI mirrors the firmware's reconnect behaviour. |
| 8 | [From scratch](08-from-scratch.md) | "If you were starting without me" — the order to build it in, what to prove first, and the pitfalls to avoid. |

---

## The one-paragraph version

A cheap incremental encoder on a machine's drive shaft produces two square-wave
signals (A and B). A SAM D21 microcontroller counts their edges to measure shaft
speed, converts that to feet-per-second of material, shows it on a local LCD,
and publishes it as a line of JSON over the network roughly once a second. The
network hardware is a **W5500**, a chip that runs the entire TCP/IP stack
itself, so the tiny MCU only has to speak **MQTT** (a lightweight
publish/subscribe messaging protocol) to a **broker** (Mosquitto, running on a
Raspberry Pi). Any number of dashboards — a Python/Tkinter app in this repo, or
Node-RED, or MQTT Explorer — subscribe to the broker and show the data live.
No dashboard talks to the node directly; the broker fans the data out.

```
 ┌──────────┐  A/B   ┌───────────┐  SPI   ┌────────┐  Ethernet  ┌──────────┐
 │ Encoder  ├───────►│  SAM D21  ├───────►│ W5500  ├───────────►│  Switch  │
 │ on shaft │ pulses │   (MCU)   │ 2 MHz  │ TCP/IP │   Cat6     │          │
 └──────────┘        └─────┬─────┘        └────────┘            └────┬─────┘
                           │ 4-bit                                   │
                           ▼                                         │
                     ┌──────────┐                                    ▼
                     │ 16x2 LCD │                        ┌───────────────────────┐
                     │ (local)  │                        │ Raspberry Pi          │
                     └──────────┘                        │  Mosquitto MQTT broker│
                                                         └───────────┬───────────┘
                                                                     │ fans out
                                            ┌────────────────────────┼───────────────┐
                                            ▼                        ▼                ▼
                                     ┌─────────────┐        ┌──────────────┐  ┌──────────────┐
                                     │ Python GUI  │        │  Node-RED    │  │ MQTT Explorer│
                                     │ (this repo) │        │  dashboard   │  │  (debugging) │
                                     └─────────────┘        └──────────────┘  └──────────────┘
```

Everything else in this guide is a zoom-in on one box or one arrow in that
diagram.

---

## A note on how this project was built

The order matters, and it's a teaching point in itself. We didn't build the
network first. We built **bring-up → LCD → encoder → telemetry → network**, each
phase proven before the next started, each with its own acceptance test. The
network — the part that feels like "the point" — came last, on top of a node
that already worked and could already tell us what it was doing over a serial
cable. That's not an accident; see [From scratch](08-from-scratch.md) for why
building in that order is the single biggest thing that made this go smoothly.
