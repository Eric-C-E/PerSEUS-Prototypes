from __future__ import annotations

import queue
import socket
import threading
from typing import Any, Callable

from perseus_shared.protocol import encode_message, parse_json_line

from .device_registry import DeviceRegistry


LogCallback = Callable[[str], None]


class TCPServer:
    def __init__(
        self,
        host: str,
        port: int,
        registry: DeviceRegistry,
        log_callback: LogCallback | None = None,
    ) -> None:
        self.host = host
        self.port = port
        self.registry = registry
        self.log_callback = log_callback
        self.gui_queue: queue.Queue[dict[str, Any]] = queue.Queue()
        self._server_socket: socket.socket | None = None
        self._running = False
        self._send_lock = threading.Lock()

    def log(self, message: str) -> None:
        print(message)
        if self.log_callback is not None:
            self.log_callback(message)

    def start(self) -> None:
        if self._running:
            return

        self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server_socket.bind((self.host, self.port))
        self._server_socket.listen()
        self._server_socket.settimeout(1.0)
        self._running = True

        thread = threading.Thread(target=self._accept_loop, daemon=True)
        thread.start()
        self.log(f"[server] Listening on {self.host}:{self.port}")

    def stop(self) -> None:
        self._running = False
        if self._server_socket is not None:
            try:
                self._server_socket.close()
            except OSError:
                pass
            self._server_socket = None
        self.log("[server] Stopped")

    def _accept_loop(self) -> None:
        assert self._server_socket is not None

        while self._running:
            try:
                conn, address = self._server_socket.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            self.log(f"[server] Connection opened from {address}")
            thread = threading.Thread(
                target=self._client_loop,
                args=(conn, address),
                daemon=True,
            )
            thread.start()

    def _client_loop(self, conn: socket.socket, address: tuple[str, int]) -> None:
        file_obj = conn.makefile("r", encoding="utf-8")
        device_id: str | None = None

        try:
            while self._running:
                try:
                    line = file_obj.readline()
                except OSError as exc:
                    self.log(f"[server] Read error from {address}: {exc}")
                    break

                if line == "":
                    self.log(f"[server] Connection closed by peer {address}")
                    break

                self.log(f"[recv] {address} {line.strip()}")
                message = parse_json_line(line)
                if message is None:
                    self.log(f"[server] Ignored bad message from {address}")
                    continue

                message_type = message.get("type")
                if message_type == "hello":
                    device_id = self._handle_hello(conn, address, message)
                elif message_type == "heartbeat":
                    if device_id is None:
                        device_id = message.get("device_id")
                    self._handle_heartbeat(device_id, message)
                elif message_type == "event":
                    if device_id is None:
                        device_id = message.get("device_id")
                    self._handle_event(device_id, message)
                else:
                    self.log(f"[server] Unknown message type from {address}: {message}")
        finally:
            try:
                file_obj.close()
            except OSError:
                pass
            try:
                conn.close()
            except OSError:
                pass

            disconnected = self.registry.disconnect_by_conn(conn)
            if disconnected is not None:
                self.log(f"[server] Disconnect: {disconnected.device_id}")
                self._notify_gui("device_list_changed")
            else:
                self.log(f"[server] Disconnect from unknown client {address}")

    def _handle_hello(
        self,
        conn: socket.socket,
        address: tuple[str, int],
        message: dict[str, Any],
    ) -> str | None:
        device_id = message.get("device_id")
        device_kind = message.get("device_kind")
        if not isinstance(device_id, str) or not isinstance(device_kind, str):
            self.log(f"[server] Invalid hello message from {address}: {message}")
            return None

        self.registry.register_hello(device_id, device_kind, conn, address)
        self.log(f"[server] Hello received: device_id={device_id} device_kind={device_kind}")
        self._notify_gui("device_list_changed")
        return device_id

    def _handle_heartbeat(self, device_id: str | None, message: dict[str, Any]) -> None:
        if not device_id:
            self.log(f"[server] Heartbeat missing device_id: {message}")
            return

        self.registry.mark_heartbeat(device_id)
        self.log(f"[server] Heartbeat received from {device_id}")
        self._notify_gui("device_list_changed")

    def _handle_event(self, device_id: str | None, message: dict[str, Any]) -> None:
        if not device_id:
            self.log(f"[server] Event missing device_id: {message}")
            return

        event_name = str(message.get("event", ""))
        self.registry.update_event(device_id, event_name)
        self.log(f"[server] Event received from {device_id}: {message}")
        self._notify_gui("device_list_changed")

    def _notify_gui(self, kind: str, **payload: Any) -> None:
        event = {"kind": kind}
        event.update(payload)
        self.gui_queue.put(event)

    def send_message(self, device_id: str, message: dict[str, Any]) -> bool:
        conn = self.registry.get_live_connection(device_id)
        if conn is None:
            self.log(f"[send] Device not connected: {device_id}")
            return False

        raw = encode_message(message)
        try:
            with self._send_lock:
                conn.sendall(raw)
        except OSError as exc:
            self.log(f"[send] Failed to send to {device_id}: {exc}")
            self.registry.disconnect_by_id(device_id)
            self._notify_gui("device_list_changed")
            return False

        self.log(f"[send] {device_id} {message}")
        return True
