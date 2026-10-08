"""Entry point for the node GUI dashboard.

Deliberately thin: it wires the three pieces together and starts them. All the
real work lives in the modules it imports, so this file stays readable top to
bottom -- you can see the whole shape of the app in one screen.

    TelemetryModel   the shared data (what to show)
    MqttClient       fills the model from the broker (the backend half)
    MainWindow       reads the model and draws it (the frontend half)

Run it:  python main.py   (see README.md for setup on Pi4 and Windows)
"""

import tkinter as tk

from telemetry_model import TelemetryModel
from mqtt_client import MqttClient
from ui.main_window import MainWindow


def main() -> None:
    # 1. The shared data object both halves talk through.
    model = TelemetryModel()

    # 2. The MQTT client fills the model in the background.
    mqtt = MqttClient(model)
    mqtt.start()

    # 3. The Tkinter window reads the model and redraws on a timer.
    root = tk.Tk()
    window = MainWindow(root, model)
    window.start_refresh()

    # Make sure the MQTT background loop is shut down cleanly when the window
    # closes, so we don't leave a dangling network thread.
    def on_close():
        mqtt.stop()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()


if __name__ == "__main__":
    main()
