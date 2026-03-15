import json
from typing import Any


VALID_STATES = [
    "neutral",
    "high_negative",
    "high_positive",
    "low_negative",
    "low_positive",
]


def encode_message(message: dict[str, Any]) -> bytes:
    """Encode one JSON message followed by a newline."""
    text = json.dumps(message, separators=(",", ":"))
    return (text + "\n").encode("utf-8")


def parse_json_line(line: str) -> dict[str, Any] | None:
    """Parse one newline-delimited JSON object.

    Returns None if the line is empty, malformed, or not a JSON object.
    """
    text = line.strip()
    if not text:
        return None

    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        print(f"[protocol] Bad JSON line: {text!r} error={exc}")
        return None

    if not isinstance(data, dict):
        print(f"[protocol] Ignoring non-dict JSON message: {data!r}")
        return None

    return data


def build_hello(device_id: str, device_kind: str) -> dict[str, Any]:
    return {
        "type": "hello",
        "device_id": device_id,
        "device_kind": device_kind,
    }


def build_heartbeat() -> dict[str, Any]:
    return {
        "type": "heartbeat",
    }


def build_event(device_id: str, name: str, **extra: Any) -> dict[str, Any]:
    message = {
        "type": "event",
        "device_id": device_id,
        "event": name,
    }
    message.update(extra)
    return message


def build_set_state_command(device_id: str, state: str) -> dict[str, Any]:
    return {
        "type": "command",
        "device_id": device_id,
        "command": "set_state",
        "state": state,
    }


def build_vibration_command(
    device_id: str,
    enabled: bool,
) -> dict[str, Any]:
    return {
        "type": "command",
        "device_id": device_id,
        "command": "set_vibration",
        "enabled": enabled,
    }


def build_vibration_level_command(
    device_id: str,
    level: float,
) -> dict[str, Any]:
    return {
        "type": "command",
        "device_id": device_id,
        "command": "set_vibration_level",
        "level": float(level),
    }


def build_flower_raw_command(
    device_id: str,
    run: bool,
    speed: float,
    amplitude: float,
) -> dict[str, Any]:
    return {
        "type": "command",
        "device_id": device_id,
        "command": "set_raw",
        "run": run,
        "speed": float(speed),
        "amplitude": float(amplitude),
    }
