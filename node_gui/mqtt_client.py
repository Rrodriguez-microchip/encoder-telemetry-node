"""MQTT side of the dashboard: connect, subscribe, parse, fill the model.

This is the "backend" half -- it knows about MQTT and JSON and nothing about
the screen. It runs the paho client's network loop on its own background
thread, so the UI stays responsive while messages arrive. Everything it learns
goes into the shared TelemetryModel; the UI reads from there.

paho-mqtt is the one external dependency (see requirements.txt). It's the same
MQTT protocol the firmware speaks via ioLibrary's Paho client -- here we're the
subscriber, the node is the publisher.
"""

import json

import paho.mqtt.client as mqtt

import config
from telemetry_model import TelemetryModel


class MqttClient:
    """Subscribes to one node's telemetry + status topics.

    Lifecycle: construct with a model, call start() once, call stop() on exit.
    paho handles reconnection automatically between start() and stop().
    """

    def __init__(self, model: TelemetryModel):
        self._model = model

        # callback_api_version keeps us on paho's modern (v2) callback
        # signatures, which are stable across paho 2.x.
        self._client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2
        )
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message

    # --- public API --------------------------------------------------------

    def start(self) -> None:
        """Connect and start the background network loop.

        connect_async + loop_start means this returns immediately and never
        blocks the UI, even if the broker is down -- paho keeps retrying in the
        background, mirroring the firmware's "keep trying, connect when the
        broker appears" behaviour.
        """
        self._client.connect_async(config.BROKER_HOST, config.BROKER_PORT)
        self._client.loop_start()

    def stop(self) -> None:
        """Stop the network loop and disconnect cleanly."""
        self._client.loop_stop()
        self._client.disconnect()

    # --- paho callbacks (run on the paho network thread) -------------------

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        """Subscribe once we're connected (and re-subscribe after a reconnect).

        Subscribing in on_connect (not in start) is the correct pattern:
        subscriptions are lost on disconnect, so doing it here restores them
        automatically every time paho reconnects.
        """
        self._model.set_broker_connected(True)
        if reason_code == 0:
            client.subscribe(config.TELEMETRY_TOPIC)
            client.subscribe(config.STATUS_TOPIC)

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        self._model.set_broker_connected(False)

    def _on_message(self, client, userdata, msg):
        """Route an incoming message to the model by topic.

        Two topics: telemetry carries the JSON payload; status carries the
        plain-text "online"/"offline" presence (retained, so we get the current
        value immediately on subscribe).
        """
        if msg.topic == config.TELEMETRY_TOPIC:
            self._handle_telemetry(msg.payload)
        elif msg.topic == config.STATUS_TOPIC:
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
