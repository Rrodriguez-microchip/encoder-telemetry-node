"""Configuration for the node GUI dashboard.

One place to edit the connection details so the rest of the app never
hard-codes them. These must match the firmware (see CLAUDE.md §5 pin map and
the net_mqtt.c / config.c defaults):

    broker IP/port .......... 192.168.1.5 : 1883   (the Pi running Mosquitto)
    topics .................. bldg/<AREA>/<NODE_ID>/telemetry
                              bldg/<AREA>/<NODE_ID>/status

Later this could be loaded from a file or a settings screen; for v1 it lives
here as plain module-level values you edit by hand.
"""

# --- MQTT broker (the Raspberry Pi running Mosquitto) ----------------------
# On the Pi itself, "localhost" works. From a Windows machine on the same LAN,
# use the Pi's IP (the firmware's broker IP is 192.168.1.5).
BROKER_HOST = "localhost"
BROKER_PORT = 1883

# --- Which node this dashboard watches -------------------------------------
# These build the topic strings and must match the firmware's config.c.
AREA = "extrusion"
NODE_ID = "node01"

# --- Derived topics (don't edit; built from the values above) --------------
TELEMETRY_TOPIC = f"bldg/{AREA}/{NODE_ID}/telemetry"
STATUS_TOPIC = f"bldg/{AREA}/{NODE_ID}/status"

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
