"""Configuration for the node GUI dashboard.

One place to edit the connection details so the rest of the app never
hard-codes them. These must match the firmware (see CLAUDE.md §5 pin map and
the net_mqtt.c / config.c defaults):

    topics .................. bldg/<AREA>/<NODE_ID>/telemetry
                              bldg/<AREA>/<NODE_ID>/status

Per-machine values (where the broker is, which node to watch) are read from
environment variables, with sensible defaults baked in. This is why the
committed file never needs editing per machine:

  - On the Pi (broker is local), the defaults just work -- run it as-is.
  - On a Windows/other machine on the LAN, set the broker to the Pi's IP ONCE
    in your shell instead of editing this file:

        Windows (PowerShell):  $env:NODE_GUI_BROKER = "192.168.50.64"
        Windows (cmd):         set NODE_GUI_BROKER=192.168.50.64
        Linux/macOS/Pi:        export NODE_GUI_BROKER=192.168.50.64

    then run `python main.py`. The IP stays out of version control, so the
    repo stays correct for every machine and every network.

Supported env vars: NODE_GUI_BROKER, NODE_GUI_PORT, NODE_GUI_AREA,
NODE_GUI_NODE_ID.
"""

import os

# --- MQTT broker (the Raspberry Pi running Mosquitto) ----------------------
# Default "localhost" is correct when the GUI runs ON the Pi. From another
# machine, set NODE_GUI_BROKER to the Pi's IP (see the module docstring) rather
# than editing this line, so the per-machine IP never gets committed.
BROKER_HOST = os.environ.get("NODE_GUI_BROKER", "localhost")
BROKER_PORT = int(os.environ.get("NODE_GUI_PORT", "1883"))

# --- Which node this dashboard watches -------------------------------------
# These build the topic strings and must match the firmware's config.c.
AREA = os.environ.get("NODE_GUI_AREA", "extrusion")
NODE_ID = os.environ.get("NODE_GUI_NODE_ID", "node01")

# --- Derived topics (don't edit; built from the values above) --------------
TELEMETRY_TOPIC = f"bldg/{AREA}/{NODE_ID}/telemetry"
STATUS_TOPIC = f"bldg/{AREA}/{NODE_ID}/status"

# --- Material geometry (MUST match the firmware) ---------------------------
# feet of material per shaft revolution, used to turn the node's revolution
# count into a "total extruded" distance. This MIRRORS the firmware's
# config.c value (CFG_FEET_PER_REV_MILLI / 1000). If you recalibrate it in the
# firmware, update it HERE TOO -- the GUI has no way to know otherwise. (A
# future firmware change could add feet_per_rev to the telemetry JSON and this
# duplicate would go away.)
FEET_PER_REV = 1.000   # placeholder, same as firmware; calibrate on the machine

# --- UI behaviour ----------------------------------------------------------
# How often the window redraws from the latest data, in milliseconds. The node
# publishes telemetry at ~1 Hz over MQTT, so 250 ms keeps the UI responsive
# without spinning. The MQTT client fills the model in the background; the UI
# just reads whatever is latest on each tick.
UI_REFRESH_MS = 250

# Seconds without any telemetry message before we show the node as "stale",
# even if the retained /status still says online. Guards against a node that
# died without the broker having fired the last-will yet (keepalive is 60 s).
STALE_AFTER_S = 5.0
