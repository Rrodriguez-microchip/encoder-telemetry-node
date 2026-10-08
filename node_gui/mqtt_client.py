"""MQTT side of the dashboard: connect, subscribe, parse, fill the model.

This is the "backend" half -- it knows about MQTT and JSON and nothing about
the screen. It runs the paho client's network loop on its own background
thread, so the UI stays responsive while messages arrive. Everything it learns
goes into the shared TelemetryModel; the UI reads from there.

paho-mqtt is the one external dependency (see requirements.txt). It's the same
MQTT protocol the firmware speaks via ioLibrary's Paho client -- here we're the
subscriber, the node is the publisher.

The broker address and topics come from a settings dict (see settings_store),
not from hard-coded config, so the Settings dialog can re-point this client at
a different broker/node at runtime via apply_settings().
"""

import json

import paho.mqtt.client as mqtt

import settings_store
from telemetry_model import TelemetryModel


class MqttClient:
    """Subscribes to one node's telemetry + status topics.

    Lifecycle: construct with a model + settings, call start() once, call stop()
    on exit. apply_settings() swaps to a new broker/node while running. paho
    handles reconnection automatically between start() and stop().
    """

    def __init__(self, model: TelemetryModel, settings: dict):
        self._model = model
        self._apply_settings_values(settings)

        # callback_api_version keeps us on paho's modern (v2) callback
        # signatures, which are stable across paho 2.x.
        self._client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2
        )
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message

    def _apply_settings_values(self, settings: dict) -> None:
        """Cache broker address + the two topic strings from a settings dict."""
        self._host = settings["broker_host"]
        self._port = int(settings["broker_port"])
        topics = settings_store.topics_for(settings["area"], settings["node_id"])
        self._telemetry_topic = topics["telemetry"]
        self._status_topic = topics["status"]

    # --- public API --------------------------------------------------------

    def start(self) -> None:
        """Connect and start the background network loop.

        connect_async + loop_start means this returns immediately and never
        blocks the UI, even if the broker is down -- paho keeps retrying in the
        background, mirroring the firmware's "keep trying, connect when the
        broker appears" behaviour.
        """
        self._client.connect_async(self._host, self._port)
        self._client.loop_start()

    def stop(self) -> None:
        """Stop the network loop and disconnect cleanly."""
        self._client.loop_stop()
        self._client.disconnect()

    def apply_settings(self, settings: dict) -> None:
        """Re-point at a new broker/node without restarting the app.

        Called when the user saves the Settings dialog. We fully tear down the
        old connection and start a fresh one at the new address -- simplest
        correct approach, and reconnection is cheap. The model's connection
        flag goes false during the gap, so the UI shows "no broker connection"
        until the new link is up, which is the honest state.
        """
        self.stop()
        self._model.set_broker_connected(False)
        self._apply_settings_values(settings)
        self.start()

    # --- paho callbacks (run on the paho network thread) -------------------

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        """Subscribe once we're connected (and re-subscribe after a reconnect).

        Subscribing in on_connect (not in start) is the correct pattern:
        subscriptions are lost on disconnect, so doing it here restores them
        automatically every time paho reconnects.
        """
        self._model.set_broker_connected(True)
        if reason_code == 0:
            client.subscribe(self._telemetry_topic)
            client.subscribe(self._status_topic)

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        self._model.set_broker_connected(False)

    def _on_message(self, client, userdata, msg):
        """Route an incoming message to the model by topic.

        Two topics: telemetry carries the JSON payload; status carries the
        plain-text "online"/"offline" presence (retained, so we get the current
        value immediately on subscribe).
        """
        if msg.topic == self._telemetry_topic:
            self._handle_telemetry(msg.payload)
        elif msg.topic == self._status_topic:
            self._handle_status(msg.payload)

    # --- payload handling --------------------------------------------------

    def _handle_telemetry(self, payload: bytes) -> None:
        """Parse the telemetry JSON and update the model.

        Wrapped in try/except so a single malformed message can never crash the
        network thread -- we just skip it and wait for the next good one.
        """
        try:
            data = json.loads(payload.decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            return
        self._model.update_telemetry(data)

    def _handle_status(self, payload: bytes) -> None:
        """Interpret the retained presence payload ("online"/"offline")."""
        text = payload.decode("utf-8", errors="replace").strip().lower()
        self._model.set_online(text == "online")
