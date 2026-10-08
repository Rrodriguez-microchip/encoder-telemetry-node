"""Load and save the customer-editable settings (broker + which node).

Settings live in a `settings.json` file next to the app, so a non-technical
user never touches a terminal, an environment variable, or hand-edits code --
they use the in-app Settings dialog, which calls save() here.

Precedence, lowest to highest:

    built-in defaults (config.py)  ->  settings.json  ->  environment variables

So:
  - a customer who only ever uses the Settings dialog gets the file layer;
  - a developer can still override with NODE_GUI_* env vars during testing;
  - if nothing is set anywhere, the config.py defaults apply.

Only broker host/port and area/node_id are stored here. feet_per_rev is
deliberately NOT customer-editable (it must match the firmware calibration), so
it stays in config.py.
"""

import json
import os

import config

# settings.json sits beside this file, i.e. inside node_gui/, so it travels
# with the app regardless of the working directory it's launched from.
SETTINGS_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "settings.json")

# The keys we persist, and where their built-in defaults come from.
_DEFAULTS = {
    "broker_host": config.BROKER_HOST,
    "broker_port": config.BROKER_PORT,
    "area": config.AREA,
    "node_id": config.NODE_ID,
}


def load() -> dict:
    """Return the effective settings, merging defaults < file < env vars.

    Never raises: a missing or corrupt settings.json falls back to defaults, so
    a bad file can't stop the app from starting (worst case the user re-enters
    the values in the Settings dialog).
    """
    settings = dict(_DEFAULTS)

    # File layer.
    try:
        with open(SETTINGS_PATH, "r", encoding="utf-8") as f:
            saved = json.load(f)
        # Only accept keys we know, so a stray/edited file can't inject junk.
        for key in _DEFAULTS:
            if key in saved:
                settings[key] = saved[key]
    except (FileNotFoundError, ValueError, OSError):
        pass  # no file yet, or unreadable/corrupt -> keep defaults

    # Env layer (highest priority; mainly a developer override).
    env = {
        "broker_host": os.environ.get("NODE_GUI_BROKER"),
        "broker_port": os.environ.get("NODE_GUI_PORT"),
        "area": os.environ.get("NODE_GUI_AREA"),
        "node_id": os.environ.get("NODE_GUI_NODE_ID"),
    }
    for key, value in env.items():
        if value is not None and value != "":
            settings[key] = value

    # Normalise types (file or env may hand us a string port).
    settings["broker_port"] = int(settings["broker_port"])

    return settings


def save(settings: dict) -> None:
    """Write the customer-editable settings to settings.json.

    Only the known keys are written. Raises OSError if the file can't be
    written (the dialog surfaces that to the user rather than failing silently).
    """
    to_write = {key: settings[key] for key in _DEFAULTS if key in settings}
    to_write["broker_port"] = int(to_write.get("broker_port", config.BROKER_PORT))
    with open(SETTINGS_PATH, "w", encoding="utf-8") as f:
        json.dump(to_write, f, indent=2)


def topics_for(area: str, node_id: str) -> dict:
    """Build the telemetry/status topic strings for a given area/node."""
    return {
        "telemetry": f"bldg/{area}/{node_id}/telemetry",
        "status": f"bldg/{area}/{node_id}/status",
    }
