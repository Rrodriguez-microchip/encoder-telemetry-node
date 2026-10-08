# Node GUI — Extrusion Telemetry Dashboard

A small cross-platform (Raspberry Pi 4 + Windows) dashboard that subscribes to
the extrusion node's MQTT telemetry and shows it live: RPM, speed (ft/s),
direction, count, revolutions, button, and an online/offline status light.

It is a **read-only viewer** for v1 — it consumes the `telemetry` and `status`
topics the firmware publishes (see the main project CLAUDE.md, P4). It does not
command the node.

---

## What it looks like / what it shows

- **Headline tiles:** RPM and Speed (ft/s) — the two numbers that matter.
- **Direction / Count / Revolutions / Total extruded:** the rest of the readout.
  "Total extruded" is distance since the last reset (revolutions × feet-per-rev),
  tared by the **Reset total** button. Reset is GUI-only — it zeroes this view's
  total; the node keeps counting and is not touched.
- **Status light** (top right):
  - 🟢 green — node online and telemetry fresh
  - 🟠 amber — `/status` says online but no telemetry for a few seconds
  - 🔴 red — node offline, or the GUI isn't connected to the broker

The amber state matters: it catches a node that died before the broker's
last-will keepalive (~60–90 s) has fired, so you see "something's wrong" sooner
than the offline flag alone would show it.

---

## How it's organised (so main.py stays clean)

```
node_gui/
├── main.py             entry point — wires the pieces together, nothing else
├── config.py           broker IP/port, area/node, topics, UI timing — edit here
├── telemetry_model.py  the shared data object both halves talk through
├── mqtt_client.py      connects/subscribes/parses (the "backend" half)
└── ui/
    ├── main_window.py  window layout + the periodic redraw (the "frontend")
    └── widgets.py      reusable display widgets (value tile, status light)
```

The MQTT side knows nothing about the screen; the UI side knows nothing about
MQTT. They meet only through `telemetry_model.py`. This mirrors the firmware's
`encoder.h` API / core / backend split: either half can be replaced without
touching the other.

---

## Setup & run

You need **Python 3.9+**.

### Windows

```bat
cd node_gui
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python main.py
```

Tkinter ships with the python.org Windows installer — no extra step.

### Raspberry Pi 4 (Debian/Raspberry Pi OS)

```bash
cd node_gui
sudo apt install python3-tk      # Tkinter is a separate system package on Debian
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 main.py
```

### Pointing it at the right broker

**The normal way — the Settings button (no terminal, no files).**
Launch the app and click **Settings**. Type the broker's IP/host (and the area
/ node you want to watch), click **Save**. The app reconnects immediately and
remembers the values for next time (written to `settings.json` next to the app).

- On the Pi that also runs Mosquitto, the default `localhost` already works —
  you may never need the dialog.
- On a Windows/other machine, open Settings once and enter the Pi's IP
  (`hostname -I` on the Pi gives it). Done.

**Developer shortcut — environment variables (optional).**
For quick testing you can override without the dialog; these win over the saved
file:

```bat
:: Windows cmd
set NODE_GUI_BROKER=192.168.50.64
python main.py
```
```bash
# Linux / macOS / Pi
export NODE_GUI_BROKER=192.168.50.64
python3 main.py
```

`settings.json` is git-ignored, so a machine's own IP never gets committed.

---

## Configuration

All knobs live in `config.py`. The per-machine ones can be overridden by an
environment variable (shown in brackets) so you don't edit the committed file:

| Setting | Env var | Meaning |
|---|---|---|
| `BROKER_HOST` | `NODE_GUI_BROKER` | where Mosquitto is (`localhost` on the Pi, else its IP) |
| `BROKER_PORT` | `NODE_GUI_PORT` | broker port (default 1883) |
| `AREA` / `NODE_ID` | `NODE_GUI_AREA` / `NODE_GUI_NODE_ID` | which node to watch; builds the topic strings |
| `FEET_PER_REV` | — | feet per shaft rev for "total extruded" — **must match firmware config.c** |
| `UI_REFRESH_MS` | — | how often the window redraws (default 250 ms) |
| `STALE_AFTER_S` | — | seconds of telemetry silence before the amber "stale" state |

These must match the firmware's `config.c` (`AREA`/`NODE_ID`) for the topics to
line up.

---

## Testing it without the node

If the hardware node isn't handy, you can fake telemetry from any machine with
Mosquitto's tools, and the dashboard will show it:

```bash
# mark the node online (retained, like the firmware does)
mosquitto_pub -h localhost -t bldg/extrusion/node01/status -r -m online

# push one telemetry sample
mosquitto_pub -h localhost -t bldg/extrusion/node01/telemetry \
  -m '{"node":"node01","count":80,"det":20,"rev":1,"dir":1,"rpm":42.5,"ft_s":0.71,"btn":0}'
```

---

## Status

**v1, not yet tested on hardware.** Committed so it can be pulled onto the Pi4
and run there; a follow-up commit will confirm it works end-to-end against the
live node.
