# GUI Workspace

This folder contains the host-side prototype controller:

- `src/wizard_gui`: Tkinter GUI, TCP server, registry, and device simulator
- `run_gui.py`: local entrypoint for the controller
- `run_fake_device.py`: local entrypoint for the Python fake device

Run from this folder with the shared Python path added:

```powershell
$env:PYTHONPATH = "src;..\..\shared\python"
python run_gui.py
python run_fake_device.py --device-id flower_01 --device-kind flower
```

## ESP32 Devices on a Computer Hotspot

The GUI TCP server listens on `0.0.0.0:9000`. That means it accepts connections on all network interfaces, including the computer's Wi-Fi hotspot adapter. The ESP32 clients should not connect to `0.0.0.0`; that address is only used by the server when binding locally.

For real ESP32 devices:

1. Start the computer's Wi-Fi hotspot.
2. Connect each ESP32 to that hotspot SSID.
3. Find the computer's hotspot IPv4 address with:

```powershell
ipconfig
```

Look for the IPv4 address on the hotspot adapter. On Windows this is commonly:

```text
192.168.137.1
```

4. Configure the ESP32 TCP client to connect to that IP on port `9000`.

Example ESP32-side configuration:

```c
#define WIFI_SSID "GOUGTOP 5437"
#define WIFI_PASSWORD "8,F49k20"
#define GUI_HOST "192.168.137.1"
#define GUI_PORT 9000
```

No GUI server code change should be required for this. Change the GUI only if port `9000` conflicts, if device discovery is added, or if authentication/encryption is added.

If the ESP32 joins the hotspot but cannot connect to the GUI, check Windows Firewall. Allow Python on private networks or add an inbound firewall rule for TCP port `9000`.
