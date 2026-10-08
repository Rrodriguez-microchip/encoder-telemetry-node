"""The Settings dialog: a small modal for the customer to set the broker + node.

This is why the customer never needs a terminal or environment variable: they
click "Settings", type the Pi's IP in a labelled field, and click Save. The
values are written to settings.json (via settings_store) and applied live.

Deliberately only exposes broker host/port and area/node_id. feet_per_rev is
NOT here -- it must match the firmware calibration, so it's set by the
installer in config.py, not by the operator.
"""

import tkinter as tk
from tkinter import messagebox

import settings_store


class SettingsDialog(tk.Toplevel):
    """Modal settings editor. Calls `on_save(new_settings)` when saved.

    Usage:
        SettingsDialog(root, current_settings, on_save=handler)

    `on_save` receives the validated settings dict; it's responsible for
    persisting and applying them (main.py wires it to save + reconnect).
    """

    def __init__(self, parent, current: dict, on_save):
        super().__init__(parent)
        self._on_save = on_save

        self.title("Settings")
        self.transient(parent)        # stay on top of the main window
        self.grab_set()               # modal: block the main window until closed
        # Give it a floor size so the button row is never clipped off the
        # bottom (which looked like "there's no Save button").
        self.minsize(320, 240)

        # One StringVar per field, pre-filled with the current value.
        self._vars = {
            "broker_host": tk.StringVar(value=str(current["broker_host"])),
            "broker_port": tk.StringVar(value=str(current["broker_port"])),
            "area": tk.StringVar(value=str(current["area"])),
            "node_id": tk.StringVar(value=str(current["node_id"])),
        }

        # Build the button bar FIRST and anchor it to the bottom, so it always
        # reserves its space and can never be pushed off-screen by the form.
        self._build_buttons()
        self._build_fields()

    def _build_fields(self) -> None:
        rows = [
            ("Broker IP / host", "broker_host"),
            ("Broker port", "broker_port"),
            ("Area", "area"),
            ("Node ID", "node_id"),
        ]
        form = tk.Frame(self, padx=16, pady=12)
        form.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        for i, (label, key) in enumerate(rows):
            tk.Label(form, text=label, anchor="w").grid(
                row=i, column=0, sticky="w", pady=4, padx=(0, 10)
            )
            tk.Entry(form, textvariable=self._vars[key], width=24).grid(
                row=i, column=1, pady=4
            )

    def _build_buttons(self) -> None:
        # side=BOTTOM so this bar is pinned to the window's bottom edge and
        # keeps its space even if the form above grows. (padx/pady on a Frame
        # must be scalars; the asymmetric bottom pad goes on pack() instead,
        # where a (top, bottom) tuple is allowed.)
        bar = tk.Frame(self, padx=16)
        bar.pack(side=tk.BOTTOM, fill=tk.X, pady=(0, 12))
        tk.Button(bar, text="Cancel", command=self.destroy).pack(side=tk.RIGHT)
        tk.Button(bar, text="Save", command=self._save).pack(
            side=tk.RIGHT, padx=(0, 8)
        )

    def _save(self) -> None:
        """Validate, persist, apply, close. Keeps the dialog open on error."""
        host = self._vars["broker_host"].get().strip()
        port_text = self._vars["broker_port"].get().strip()
        area = self._vars["area"].get().strip()
        node_id = self._vars["node_id"].get().strip()

        # Minimal validation: non-empty host/area/node and a numeric port. We
        # don't verify the broker is reachable here -- the status light will
        # show that once we try to connect, which is the right place for it.
        if not host or not area or not node_id:
            messagebox.showerror("Settings", "Host, Area and Node ID can't be empty.")
            return
        try:
            port = int(port_text)
            if not (1 <= port <= 65535):
                raise ValueError
        except ValueError:
            messagebox.showerror("Settings", "Port must be a number from 1 to 65535.")
            return

        new_settings = {
            "broker_host": host,
            "broker_port": port,
            "area": area,
            "node_id": node_id,
        }

        try:
            settings_store.save(new_settings)
        except OSError as exc:
            messagebox.showerror("Settings", f"Couldn't save settings file:\n{exc}")
            return

        self._on_save(new_settings)
        self.destroy()
