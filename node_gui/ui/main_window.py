"""The dashboard window: layout + the periodic redraw.

This is the "frontend" -- it builds the window from the widgets in widgets.py
and, on a timer, reads the latest snapshot from the model and paints it. It
knows nothing about MQTT; it's handed a model and just displays whatever is in
it. The MQTT client fills that model from another thread.

Why a timer instead of redrawing on each message: Tkinter is single-threaded
and not safe to touch from the MQTT thread. So the MQTT thread only writes to
the (thread-safe) model, and the UI polls the model on Tkinter's own loop via
`after()`. This cleanly separates "data arrives" from "screen updates".
"""

import tkinter as tk

import config
from telemetry_model import TelemetryModel
from ui.widgets import ValueTile, StatusLight


class MainWindow:
    """Builds and runs the dashboard for one node."""

    def __init__(self, root: tk.Tk, model: TelemetryModel):
        self._root = root
        self._model = model

        root.title(f"Extrusion Node — {config.AREA}/{config.NODE_ID}")
        root.minsize(560, 320)

        self._build_header()
        self._build_tiles()

    # --- layout ------------------------------------------------------------

    def _build_header(self) -> None:
        """Top strip: which node, and the status light."""
        header = tk.Frame(self._root, pady=6)
        header.pack(fill=tk.X)

        tk.Label(
            header,
            text=f"{config.AREA} / {config.NODE_ID}",
            font=("Segoe UI", 14, "bold"),
        ).pack(side=tk.LEFT, padx=12)

        self._status = StatusLight(header)
        self._status.pack(side=tk.RIGHT)

    def _build_tiles(self) -> None:
        """Grid of live value tiles. RPM and ft/s are the headline numbers."""
        grid = tk.Frame(self._root, pady=8)
        grid.pack(expand=True, fill=tk.BOTH)

        # Make the three columns share width evenly.
        for col in range(3):
            grid.columnconfigure(col, weight=1)

        self._tile_rpm = ValueTile(grid, "RPM")
        self._tile_fts = ValueTile(grid, "Speed", unit=" ft/s")
        self._tile_dir = ValueTile(grid, "Direction")
        self._tile_count = ValueTile(grid, "Count")
        self._tile_rev = ValueTile(grid, "Revolutions")
        self._tile_btn = ValueTile(grid, "Button")

        # Row 0: the headline readings. Row 1: the raw/diagnostic ones.
        self._tile_rpm.grid(row=0, column=0, sticky="nsew")
        self._tile_fts.grid(row=0, column=1, sticky="nsew")
        self._tile_dir.grid(row=0, column=2, sticky="nsew")
        self._tile_count.grid(row=1, column=0, sticky="nsew")
        self._tile_rev.grid(row=1, column=1, sticky="nsew")
        self._tile_btn.grid(row=1, column=2, sticky="nsew")

    # --- periodic redraw ---------------------------------------------------

    def start_refresh(self) -> None:
        """Kick off the self-rescheduling redraw loop."""
        self._refresh()

    def _refresh(self) -> None:
        """Read one snapshot, paint it, and schedule the next redraw.

        This is the UI-side mirror of the firmware superloop's periodic tasks:
        it runs every UI_REFRESH_MS via Tkinter's after(), reads the latest
        values, and never blocks.
        """
        snap = self._model.snapshot()

        self._tile_rpm.set_value(f"{snap['rpm']:.1f}")
        self._tile_fts.set_value(f"{snap['ft_s']:.3f}")
        self._tile_dir.set_value(self._direction_text(snap["direction"]))
        self._tile_count.set_value(str(snap["count"]))
        self._tile_rev.set_value(str(snap["revolutions"]))
        self._tile_btn.set_value("pressed" if snap["button"] else "—")

        self._update_status(snap)

        self._root.after(config.UI_REFRESH_MS, self._refresh)

    @staticmethod
    def _direction_text(direction: int) -> str:
        if direction > 0:
            return "▲ fwd"
        if direction < 0:
            return "▼ rev"
        return "— stop"

    def _update_status(self, snap: dict) -> None:
        """Translate connection/presence/freshness into the status light."""
        if not snap["connected_to_broker"]:
            self._status.set_state("red", "no broker connection")
        elif not snap["online"]:
            self._status.set_state("red", "node offline")
        elif snap["stale"]:
            self._status.set_state("amber", "online, telemetry stale")
        else:
            self._status.set_state("green", "online")
