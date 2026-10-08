"""The data the dashboard shows, as one plain object.

This is the neutral meeting point between the MQTT side (which fills it in) and
the UI side (which reads it) -- neither needs to know about the other. It's the
same decoupling idea as the firmware's encoder.h API sitting between the core
logic and the hardware backend: the UI could be swapped (Tkinter -> something
else) and the MQTT client could be swapped, as long as both still speak through
this object.

It holds the latest values only (not history) -- a live readout, not a log.
"""

import threading
import time
from dataclasses import dataclass, field


@dataclass
class TelemetryModel:
    """Latest known state of one node.

    The MQTT client updates this from its background thread; the UI reads it
    from the Tkinter main thread. Those are different threads, so every access
    goes through `_lock` to avoid reading a half-updated set of values.
    """

    # Latest telemetry fields (match the firmware JSON payload shape).
    count: int = 0          # raw encoder counts
    detents: int = 0        # KY-040 detents (count / 4)
    revolutions: int = 0    # full shaft revolutions
    direction: int = 0      # -1 / 0 / +1
    rpm: float = 0.0        # revolutions per minute
    ft_s: float = 0.0       # feet per second of material (0 until calibrated)
    button: int = 0         # encoder push-button, 0/1

    # Presence + freshness, derived from the /status topic and arrival times.
    online: bool = False            # last retained /status was "online"
    last_telemetry_ts: float = 0.0  # time.monotonic() of last telemetry msg
    connected_to_broker: bool = False  # our own MQTT socket is up

    # Tare baseline for "total extruded". The node counts continuously; the
    # dashboard shows distance *since the last reset* by subtracting the
    # revolution count captured when Reset was pressed. GUI-only -- the node is
    # untouched. None means "not tared yet", so we tare to the first sample.
    _rev_baseline: "int | None" = None

    _lock: threading.Lock = field(default_factory=threading.Lock, repr=False)

    def update_telemetry(self, data: dict) -> None:
        """Replace the telemetry fields from a parsed JSON payload.

        Unknown/missing keys keep their previous value, so a malformed message
        can't blank the display. Called from the MQTT thread.
        """
        with self._lock:
            self.count = int(data.get("count", self.count))
            self.detents = int(data.get("det", self.detents))
            self.revolutions = int(data.get("rev", self.revolutions))
            self.direction = int(data.get("dir", self.direction))
            self.rpm = float(data.get("rpm", self.rpm))
            self.ft_s = float(data.get("ft_s", self.ft_s))
            self.button = int(data.get("btn", self.button))
            self.last_telemetry_ts = time.monotonic()

            # First sample we ever see sets the tare baseline, so "total
            # extruded" starts at ~0 when the dashboard opens rather than
            # showing the node's lifetime count.
            if self._rev_baseline is None:
                self._rev_baseline = self.revolutions

    def reset_total(self) -> None:
        """Zero the 'total extruded' reading (tare).

        Captures the current revolution count as the new baseline. GUI-only:
        the node keeps counting; only this view's total resets. Called from the
        UI thread when Reset is pressed.
        """
        with self._lock:
            self._rev_baseline = self.revolutions

    def set_online(self, online: bool) -> None:
        """Record the latest /status presence. Called from the MQTT thread."""
        with self._lock:
            self.online = online

    def reset_for_new_node(self) -> None:
        """Clear all readings when the dashboard is re-pointed at another node.

        The old count/rpm/tare belong to the previous node, so showing them
        against the new one would be misleading. Blank everything and drop the
        tare baseline; the first sample from the new node re-tares. Called from
        the UI thread when the Settings dialog switches area/node_id.
        """
        with self._lock:
            self.count = 0
            self.detents = 0
            self.revolutions = 0
            self.direction = 0
            self.rpm = 0.0
            self.ft_s = 0.0
            self.button = 0
            self.online = False
            self.last_telemetry_ts = 0.0
            self._rev_baseline = None

    def set_broker_connected(self, connected: bool) -> None:
        """Record whether our own MQTT connection is up."""
        with self._lock:
            self.connected_to_broker = connected

    def snapshot(self) -> dict:
        """Return a consistent copy of everything the UI needs to draw.

        Taking one locked copy means the UI never sees a mix of old and new
        values. Also computes `stale` (telemetry gone quiet) here so the UI
        doesn't have to know the timing rule.
        """
        from config import STALE_AFTER_S, FEET_PER_REV

        with self._lock:
            age = time.monotonic() - self.last_telemetry_ts
            stale = (self.last_telemetry_ts == 0.0) or (age > STALE_AFTER_S)

            # Total extruded since the last reset = revolutions past the
            # baseline x feet-per-rev. Zero until we've taken a baseline.
            if self._rev_baseline is None:
                total_ft = 0.0
            else:
                total_ft = (self.revolutions - self._rev_baseline) * FEET_PER_REV

            return {
                "count": self.count,
                "detents": self.detents,
                "revolutions": self.revolutions,
                "direction": self.direction,
                "rpm": self.rpm,
                "ft_s": self.ft_s,
                "button": self.button,
                "total_ft": total_ft,
                "online": self.online,
                "connected_to_broker": self.connected_to_broker,
                "stale": stale,
            }
