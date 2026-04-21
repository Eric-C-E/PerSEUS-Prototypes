# Wizard-of-Oz Controller (GUI)

## Overview

A desktop GUI application that acts as a central controller for networked devices in the PerSEUS project. Devices connect as TCP clients to this server, allowing manual control ("wizard-of-oz" style) for testing and development before full automation.

The GUI does not assign device IDs. Each device carries and announces its own `device_id` in the initial `hello` message. The GUI uses that announced ID as the in-memory registry key and as the target ID in outbound commands.

## Architecture

```text
+------------------+      TCP       +------------------+
| WizardGUI        |<-------------->| Device           |
| TCP server       |    port 9000   | TCP client       |
+------------------+                +------------------+
```

## Components

### 1. TCPServer (`server.py`)

- **Role**: Listens on `0.0.0.0:9000` for incoming device connections.
- **Protocol**: JSON newline-delimited messages over TCP.
- **Concurrency**: Accept loop runs in a daemon thread and each client connection gets its own daemon thread.
- **Message Types**:
  - `hello` - Device registration with `device_id` and `device_kind`.
  - `heartbeat` - Keep-alive ping. Updates the device heartbeat timestamp.
  - `event` - Device feedback or state changes.
  - `command` - Server-to-device control messages.

### 2. DeviceRegistry (`device_registry.py`)

- **Role**: In-memory store of known devices.
- **Tracks**: `device_id`, `device_kind`, connection status, socket, address, `last_seen`, `last_heartbeat`, `last_event`, and `reset_requested`.
- **Thread-safe**: Uses a lock for concurrent server/GUI access.
- **Reset feedback**: An incoming event named `reset_requested` or `reset_request` latches the device's `reset_requested` flag to `True`.

### 3. WizardGUI (`gui.py`)

- **Role**: Tkinter-based desktop application.
- **Features**:
  - Device list panel showing known devices.
  - Green/gray circular heartbeat indicator beside each device.
  - Dynamic control panel based on selected device type.
  - Per-device reset-request light in the controls section.
  - Debug log panel.
  - Periodic GUI refresh from the server event queue.

Heartbeat indicator behavior:

- Green means the device is connected and sent a heartbeat within `HEARTBEAT_ACTIVE_SECONDS` seconds.
- Current threshold is 12 seconds.
- Gray means disconnected, no heartbeat received yet, or heartbeat is stale.

Reset-request light behavior:

- Appears in the controls section for every selected device type.
- Gray/off by default.
- Red/on once that selected device sends `event=reset_requested` or `event=reset_request`.
- Cleared when the WOO operator sends the next command to that same device.
- This flag is held only in memory and is not persisted.

### 4. Protocol (`shared/python/perseus_shared/protocol.py`)

- **Role**: Shared message encoding/decoding and message builders.
- **Encoding**: Compact JSON object followed by `\n`.
- **Valid States**: `neutral`, `high_negative`, `high_positive`, `low_negative`, `low_positive`.
- **Commands**:
  - `set_state` - Set emotional state.
  - `set_vibration` - Enable/disable vibration for `abstract_sphere`.
  - `set_vibration_level` - Set vibration intensity as a float, normally 0.0-1.0.
  - `set_raw` - Raw motor control for `flower` device with `run`, `speed`, and `amplitude`.

Note: The shared builders currently construct messages but do not validate or clamp values. The GUI constrains normal user input through buttons/sliders.

### 5. Fake Device (`fake_device.py`)

- **Role**: Testing tool that simulates a device connection.
- **Usage**: `python run_fake_device.py --device-id test1 --device-kind anthro_sphere`
- **Options**: Periodic heartbeats, periodic `moved` events, keyboard-triggered events, and a selectable keyboard event name.

## Supported Device Types

| Device Kind | Controls |
|-------------|----------|
| `anthro_sphere` | Reset-request light + state buttons |
| `abstract_sphere` | Reset-request light + state buttons + hold-to-vibrate button + vibration level |
| `flower` | Reset-request light + state buttons + raw motor controls (run/speed/amplitude) |

## Running the Application

### Start the GUI (server)

From the repository root:

```powershell
$env:PYTHONPATH = "apps\gui\src;shared\python"
python apps\gui\run_gui.py
```

Or from `apps/gui`:

```powershell
cd apps/gui
$env:PYTHONPATH = "src;..\..\shared\python"
python run_gui.py
```

### Connect a fake device for testing

```powershell
cd apps/gui
$env:PYTHONPATH = "src;..\..\shared\python"
python run_fake_device.py --device-id test1 --device-kind abstract_sphere --keyboard-events
```

### Test reset-request feedback

```powershell
cd apps/gui
$env:PYTHONPATH = "src;..\..\shared\python"
python run_fake_device.py --device-id test1 --device-kind abstract_sphere --keyboard-events --keyboard-event-name reset_requested
```

Press Enter in the fake-device terminal to send the reset request event.

## Network Protocol Details

### Device -> Server Messages

**hello** (initial registration):

```json
{"type": "hello", "device_id": "device-001", "device_kind": "anthro_sphere"}
```

**heartbeat** (keep-alive):

```json
{"type": "heartbeat", "device_id": "device-001"}
```

The server also accepts a heartbeat without `device_id` after a valid `hello`, because the connection thread remembers the device ID. The fake device sends `device_id` explicitly.

**event** (movement feedback):

```json
{"type": "event", "device_id": "device-001", "event": "moved"}
```

**event** (reset-request feedback):

```json
{"type": "event", "device_id": "device-001", "event": "reset_requested"}
```

`reset_request` is also accepted for compatibility.

### Server -> Device Commands

**set_state**:

```json
{"type": "command", "device_id": "device-001", "command": "set_state", "state": "high_positive"}
```

**set_vibration**:

```json
{"type": "command", "device_id": "device-001", "command": "set_vibration", "enabled": true}
```

For `abstract_sphere`, the GUI uses this as a hold command:

- Mouse/button press sends `enabled: true`.
- Mouse/button release or pointer leaving the button sends `enabled: false`.

**set_vibration_level**:

```json
{"type": "command", "device_id": "device-001", "command": "set_vibration_level", "level": 0.75}
```

**set_raw** (flower only):

```json
{"type": "command", "device_id": "flower-001", "command": "set_raw", "run": true, "speed": 0.5, "amplitude": 0.8}
```

## Current Limitations

- No authentication or encryption.
- No persistent storage of device state.
- Reset-request flags are held only in memory and clear on the next WOO operator command to that device. There is no protocol-level clear/ack command.
- The GUI event loop is single-threaded; the server uses background threads.
- No client-side reconnection logic in the fake device.
- `device_id` uniqueness is assumed. If two clients announce the same `device_id`, they target the same registry entry.
