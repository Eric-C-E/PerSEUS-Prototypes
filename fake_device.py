from __future__ import annotations

import argparse
import socket
import sys
import threading
import time

from protocol import build_event, build_heartbeat, build_hello, encode_message, parse_json_line


def recv_loop(sock: socket.socket) -> None:
    file_obj = sock.makefile("r", encoding="utf-8")
    try:
        while True:
            line = file_obj.readline()
            if line == "":
                print("[fake_device] Server closed connection")
                break
            print(f"[fake_device] raw recv: {line.strip()}")
            message = parse_json_line(line)
            if message is None:
                print("[fake_device] bad message ignored")
                continue
            print(f"[fake_device] parsed command: {message}")
    except OSError as exc:
        print(f"[fake_device] recv error: {exc}")
    finally:
        try:
            file_obj.close()
        except OSError:
            pass


def heartbeat_loop(sock: socket.socket, device_id: str, interval: float) -> None:
    while True:
        time.sleep(interval)
        try:
            message = build_heartbeat()
            message["device_id"] = device_id
            sock.sendall(encode_message(message))
            print(f"[fake_device] sent heartbeat: {message}")
        except OSError as exc:
            print(f"[fake_device] heartbeat stopped: {exc}")
            return


def event_loop(sock: socket.socket, device_id: str, interval: float) -> None:
    while True:
        time.sleep(interval)
        try:
            message = build_event(device_id, "moved")
            sock.sendall(encode_message(message))
            print(f"[fake_device] sent event: {message}")
        except OSError as exc:
            print(f"[fake_device] event loop stopped: {exc}")
            return


def keyboard_loop(sock: socket.socket, device_id: str) -> None:
    print("[fake_device] Press Enter to send a moved event. Ctrl+C to quit.")
    while True:
        try:
            input()
        except EOFError:
            return

        try:
            message = build_event(device_id, "moved")
            sock.sendall(encode_message(message))
            print(f"[fake_device] sent event: {message}")
        except OSError as exc:
            print(f"[fake_device] keyboard loop stopped: {exc}")
            return


def main() -> None:
    parser = argparse.ArgumentParser(description="Fake TCP device for the Wizard-of-Oz controller.")
    parser.add_argument("--device-id", required=True)
    parser.add_argument(
        "--device-kind",
        required=True,
        choices=["anthro_sphere", "abstract_sphere", "flower"],
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument(
        "--event-interval",
        type=float,
        default=0.0,
        help="If > 0, send a moved event every N seconds.",
    )
    parser.add_argument(
        "--heartbeat-interval",
        type=float,
        default=5.0,
        help="Send a heartbeat every N seconds.",
    )
    parser.add_argument(
        "--keyboard-events",
        action="store_true",
        help="Send a moved event when Enter is pressed.",
    )
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print(f"[fake_device] Connecting to {(args.host, args.port)}")
    sock.connect((args.host, args.port))
    print("[fake_device] Connected")

    hello = build_hello(args.device_id, args.device_kind)
    sock.sendall(encode_message(hello))
    print(f"[fake_device] sent hello: {hello}")

    recv_thread = threading.Thread(target=recv_loop, args=(sock,), daemon=True)
    recv_thread.start()

    heartbeat_thread = threading.Thread(
        target=heartbeat_loop,
        args=(sock, args.device_id, args.heartbeat_interval),
        daemon=True,
    )
    heartbeat_thread.start()

    if args.event_interval > 0:
        event_thread = threading.Thread(
            target=event_loop,
            args=(sock, args.device_id, args.event_interval),
            daemon=True,
        )
        event_thread.start()

    if args.keyboard_events:
        keyboard_loop(sock, args.device_id)
    else:
        try:
            while recv_thread.is_alive():
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("[fake_device] Interrupted by user")

    try:
        sock.close()
    except OSError:
        pass


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
