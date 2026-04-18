import tkinter as tk

from .device_registry import DeviceRegistry
from .gui import WizardGUI
from .server import TCPServer


def main() -> None:
    host = "0.0.0.0"
    port = 9000

    registry = DeviceRegistry()
    server = TCPServer(host=host, port=port, registry=registry)
    server.start()

    root = tk.Tk()
    app = WizardGUI(root, registry, server)

    def on_close() -> None:
        server.stop()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    app.log(f"[main] GUI ready. Server on {host}:{port}")
    root.mainloop()


if __name__ == "__main__":
    main()
