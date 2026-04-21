from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
import socket
import threading
from typing import Any


def now_iso() -> str:
    return datetime.now().isoformat(timespec="seconds")


@dataclass
class DeviceRecord:
    device_id: str
    device_kind: str
    connected: bool = False
    conn: socket.socket | None = None
    address: tuple[str, int] | None = None
    last_seen: str = field(default_factory=now_iso)
    last_heartbeat: str = ""
    last_event: str = ""
    reset_requested: bool = False

    def to_dict(self) -> dict[str, Any]:
        return {
            "device_id": self.device_id,
            "device_kind": self.device_kind,
            "connected": self.connected,
            "last_seen": self.last_seen,
            "last_heartbeat": self.last_heartbeat,
            "last_event": self.last_event,
            "reset_requested": self.reset_requested,
        }


class DeviceRegistry:
    def __init__(self) -> None:
        self._devices: dict[str, DeviceRecord] = {}
        self._lock = threading.Lock()

    def register_hello(
        self,
        device_id: str,
        device_kind: str,
        conn: socket.socket,
        address: tuple[str, int],
    ) -> DeviceRecord:
        with self._lock:
            device = self._devices.get(device_id)
            if device is None:
                device = DeviceRecord(device_id=device_id, device_kind=device_kind)
                self._devices[device_id] = device

            device.device_kind = device_kind
            device.connected = True
            device.conn = conn
            device.address = address
            device.last_seen = now_iso()
            return device

    def mark_seen(self, device_id: str) -> None:
        with self._lock:
            device = self._devices.get(device_id)
            if device is not None:
                device.last_seen = now_iso()

    def mark_heartbeat(self, device_id: str) -> None:
        with self._lock:
            device = self._devices.get(device_id)
            if device is not None:
                timestamp = now_iso()
                device.last_seen = timestamp
                device.last_heartbeat = timestamp

    def update_event(self, device_id: str, event_name: str) -> None:
        with self._lock:
            device = self._devices.get(device_id)
            if device is not None:
                device.last_seen = now_iso()
                device.last_event = event_name
                if event_name in {"reset_requested", "reset_request"}:
                    device.reset_requested = True

    def clear_reset_requested(self, device_id: str) -> bool:
        with self._lock:
            device = self._devices.get(device_id)
            if device is None or not device.reset_requested:
                return False

            device.reset_requested = False
            device.last_seen = now_iso()
            return True

    def disconnect_by_id(self, device_id: str) -> None:
        with self._lock:
            device = self._devices.get(device_id)
            if device is not None:
                device.connected = False
                device.conn = None
                device.address = None
                device.last_seen = now_iso()

    def disconnect_by_conn(self, conn: socket.socket) -> DeviceRecord | None:
        with self._lock:
            for device in self._devices.values():
                if device.conn is conn:
                    device.connected = False
                    device.conn = None
                    device.address = None
                    device.last_seen = now_iso()
                    return device
        return None

    def get(self, device_id: str) -> DeviceRecord | None:
        with self._lock:
            device = self._devices.get(device_id)
            if device is None:
                return None
            return DeviceRecord(**device.to_dict(), conn=device.conn, address=device.address)

    def get_live_connection(self, device_id: str) -> socket.socket | None:
        with self._lock:
            device = self._devices.get(device_id)
            if device is None or not device.connected:
                return None
            return device.conn

    def list_devices(self) -> list[dict[str, Any]]:
        with self._lock:
            devices = [device.to_dict() for device in self._devices.values()]
        devices.sort(key=lambda item: item["device_id"])
        return devices
