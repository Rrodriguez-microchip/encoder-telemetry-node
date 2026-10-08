"""Small reusable Tkinter widgets for the dashboard.

Keeping these here means main_window.py describes the *layout* (what goes
where) without being cluttered by the *construction* of each piece. These are
the "frontend" building blocks -- they know nothing about MQTT; they're just
told what to display.
"""

import tkinter as tk


class ValueTile(tk.Frame):
    """A labelled value box: a small caption above a large value.

    Used for each live reading (RPM, ft/s, count, ...). Call set_value() to
    update the number; the caption is fixed at construction.
    """

    def __init__(self, parent, caption: str, unit: str = "", **kwargs):
        super().__init__(parent, padx=12, pady=8, **kwargs)

        self._caption = tk.Label(self, text=caption, font=("Segoe UI", 10))
        self._caption.pack()

        self._value = tk.Label(self, text="--", font=("Segoe UI", 28, "bold"))
        self._value.pack()

        self._unit = unit

    def set_value(self, text: str) -> None:
        """Update the displayed value (unit is appended if one was set)."""
        self._value.config(text=f"{text}{self._unit}" if self._unit else text)


class StatusLight(tk.Frame):
    """A coloured dot + text showing node presence/connection state.

    Three states we care about:
        green  = node online and telemetry fresh
        amber  = online per /status but telemetry has gone stale
        red    = node offline, or we're not connected to the broker
    """

    _COLORS = {
        "green": "#2e9e4f",
        "amber": "#d8a000",
        "red": "#c23b22",
        "grey": "#888888",
    }

    def __init__(self, parent, **kwargs):
        super().__init__(parent, padx=12, pady=8, **kwargs)

        # A canvas is the simplest way to draw a filled circle in Tkinter.
        self._canvas = tk.Canvas(self, width=18, height=18, highlightthickness=0)
        self._dot = self._canvas.create_oval(2, 2, 16, 16, fill=self._COLORS["grey"])
        self._canvas.pack(side=tk.LEFT)

        self._label = tk.Label(self, text="connecting...", font=("Segoe UI", 11))
        self._label.pack(side=tk.LEFT, padx=(8, 0))

    def set_state(self, color: str, text: str) -> None:
        """Set the dot colour (by name) and the status text beside it."""
        self._canvas.itemconfig(self._dot, fill=self._COLORS.get(color, "#888888"))
        self._label.config(text=text)
