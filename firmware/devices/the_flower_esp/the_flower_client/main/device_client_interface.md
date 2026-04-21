# Device Client Interface for Wizard GUI

This document is for the coding agent implementing the physical client devices for the PerSEUS Wizard-of-Oz GUI. The GUI is the TCP server. Each physical device is a TCP client that registers itself, keeps the connection alive, receives control commands, and reports simple feedback events.

## Transport

- Host: GUI machine IP address.
- Port: `9000`.
- Socket type: TCP client connection from device to GUI.
- Encoding: UTF-8 JSON object followed by one newline byte, `\n`.
- Framing: one complete JSON message per line. Do not send pretty-printed or multi-line JSON.
- Direction:
  - Device to GUI: `hello`, `heartbeat`, `event`.
  - GUI to device: `command`.

Example encoded payload:

```json
{"type":"heartbeat","device_id":"abstract_01"}
```

The actual bytes sent on the socket must be:

```text
{"type":"heartbeat","device_id":"abstract_01"}\n
```

## Device Registration

Immediately after opening the TCP connection, the device must send one `hello` message.

```json
{"type":"hello","device_id":"device-001","device_kind":"anthro_sphere"}
```

Fields:

| Field | Type | Required | Notes |
|---|---:|---:|---|
| `type` | string | yes | Must be `hello`. |
| `device_id` | string | yes | Stable unique ID chosen by the device firmware/config. The GUI does not assign IDs. |
| `device_kind` | string | yes | One of `flower`, `anthro_sphere`, `abstract_sphere`. |

Rules:

- `device_id` is the GUI registry key and the target ID used in commands.
- Do not connect two physical clients with the same `device_id`; the GUI assumes uniqueness.
- If the socket reconnects, send `hello` again on the new connection.

## Heartbeat

Send a heartbeat periodically while connected.

```json
{"type":"heartbeat","device_id":"device-001"}
```

Fields:

| Field | Type | Required | Notes |
|---|---:|---:|---|
| `type` | string | yes | Must be `heartbeat`. |
| `device_id` | string | recommended | The GUI can infer this after `hello`, but the reference fake device sends it explicitly. |

Timing:

- Recommended interval: `5` seconds.
- The GUI heartbeat indicator turns stale after `12` seconds without a heartbeat.
- Keep sending heartbeats even if no commands are received.

## Device Events

Devices may send events to update GUI feedback.

```json
{"type":"event","device_id":"device-001","event":"moved"}
```

Fields:

| Field | Type | Required | Notes |
|---|---:|---:|---|
| `type` | string | yes | Must be `event`. |
| `device_id` | string | recommended | The GUI can infer this after `hello`, but include it for clarity. |
| `event` | string | yes | Event name. |

Known event names:

| Event | GUI behavior |
|---|---|
| `moved` | Shows as the device's latest event in the device list. |
| `reset_requested` | Turns the selected device's reset-request indicator red. |
| `reset_request` | Also accepted as a reset-request alias. |

The reset-request flag is held in GUI memory and clears when the WOO operator sends the next command to that same device. There is no clear or acknowledge command in the current protocol.

## Common GUI Commands

All three device kinds receive `command` messages from the GUI. The client should parse every incoming line as JSON and ignore unknown commands without crashing.

Every command includes:

| Field | Type | Notes |
|---|---:|---|
| `type` | string | Always `command`. |
| `device_id` | string | Target device ID. Should match this client's announced `device_id`. |
| `command` | string | Command name. |

If a client receives a command where `device_id` does not match its own ID, it should ignore it.

### `set_state`

Used by `flower`, `anthro_sphere`, and `abstract_sphere`.

```json
{"type":"command","device_id":"device-001","command":"set_state","state":"high_positive"}
```

Fields:

| Field | Type | Values |
|---|---:|---|
| `state` | string | `neutral`, `high_negative`, `high_positive`, `low_negative`, `low_positive` |

Client responsibility:

- Map each state to the device's local expression or behavior.
- Treat unknown state strings as invalid and ignore them.
- The GUI does not send a duration; state remains active until another command or local firmware policy changes it.

## Anthro Sphere Interface

Register with:

```json
{"type":"hello","device_id":"anthro_01","device_kind":"anthro_sphere"}
```

GUI controls available:

| Control | Command sent to device |
|---|---|
| State buttons | `set_state` |
| Reset-request light | Driven by device event `reset_requested` or `reset_request` |

Commands the anthro sphere must implement:

| Command | Required | Payload |
|---|---:|---|
| `set_state` | yes | `state: string` |

Recommended event support:

| Event | When to send |
|---|---|
| `moved` | When the device detects relevant movement or interaction. |
| `reset_requested` | When a local reset button, sensor gesture, or fault condition requests operator attention. |

Example command handling:

```json
{"type":"command","device_id":"anthro_01","command":"set_state","state":"low_negative"}
```

## Abstract Sphere Interface

Register with:

```json
{"type":"hello","device_id":"abstract_01","device_kind":"abstract_sphere"}
```

GUI controls available:

| Control | Command sent to device |
|---|---|
| State buttons | `set_state` |
| Hold-to-vibrate button press | `set_vibration` with `enabled: true` |
| Hold-to-vibrate button release or pointer leave | `set_vibration` with `enabled: false` |
| Vibration level slider send button | `set_vibration_level` |
| Reset-request light | Driven by device event `reset_requested` or `reset_request` |

Commands the abstract sphere must implement:

| Command | Required | Payload |
|---|---:|---|
| `set_state` | yes | `state: string` |
| `set_vibration` | yes | `enabled: boolean` |
| `set_vibration_level` | yes | `level: number`, normally `0.0` to `1.0` |

### `set_vibration`

```json
{"type":"command","device_id":"abstract_01","command":"set_vibration","enabled":true}
```

Fields:

| Field | Type | Values |
|---|---:|---|
| `enabled` | boolean | `true` starts vibration, `false` stops vibration |

Client responsibility:

- Start vibration immediately on `enabled: true`.
- Stop vibration immediately on `enabled: false`.
- For safety, stop vibration if the socket disconnects or if no valid command/heartbeat supervision policy is met locally.

### `set_vibration_level`

```json
{"type":"command","device_id":"abstract_01","command":"set_vibration_level","level":0.75}
```

Fields:

| Field | Type | Values |
|---|---:|---|
| `level` | number | GUI slider sends `0.0` to `1.0` |

Client responsibility:

- Clamp `level` locally to `[0.0, 1.0]`.
- Apply the level to subsequent and/or current vibration according to firmware behavior.
- Default GUI value is `0.65`, but firmware should have its own safe startup default.

Recommended event support:

| Event | When to send |
|---|---|
| `moved` | When the sphere detects movement or interaction. |
| `reset_requested` | When local reset or operator attention is needed. |

## Flower Interface

Register with:

```json
{"type":"hello","device_id":"flower_01","device_kind":"flower"}
```

GUI controls available:

| Control | Command sent to device |
|---|---|
| State buttons | `set_state` |
| Raw flower rotation controls: run checkbox plus speed/amplitude sliders | `set_raw` |
| Flower tilt control slider | `set_tilt` |
| Reset-request light | Driven by device event `reset_requested` or `reset_request` |

Commands the flower must implement:

| Command | Required | Payload |
|---|---:|---|
| `set_state` | yes | `state: string` |
| `set_raw` | yes | `run: boolean`, `speed: number`, `amplitude: number` |
| `set_tilt` | yes | `tilt: number` |

### `set_raw` Rotation Command

```json
{"type":"command","device_id":"flower_01","command":"set_raw","run":true,"speed":0.5,"amplitude":0.8}
```

Fields:

| Field | Type | Values |
|---|---:|---|
| `run` | boolean | `true` enables the raw motion behavior, `false` disables/stops it |
| `speed` | number | GUI slider sends `0.0` to `1.0` |
| `amplitude` | number | GUI slider sends `0.0` to `1.0` |

Client responsibility:

- Clamp `speed` and `amplitude` locally to `[0.0, 1.0]`.
- Stop or idle the raw motor behavior when `run` is `false`.
- Use safe motor limits independent of GUI input; the GUI builders do not validate or clamp values.
- Decide locally how `set_state` interacts with raw mode. A conservative policy is for `set_raw` to override direct motor output while active and for `set_state` to update the desired emotional behavior for when raw mode is not active.

### `set_tilt`

```json
{"type":"command","device_id":"flower_01","command":"set_tilt","tilt":0.5}
```

Fields:

| Field | Type | Values |
|---|---:|---|
| `tilt` | number | GUI slider sends `0.0` to `1.0` |

Client responsibility:

- Clamp `tilt` locally to `[0.0, 1.0]`.
- Map the normalized tilt value to the flower's safe physical tilt range.
- Use safe actuator limits independent of GUI input; the GUI builder does not validate or clamp values.

Recommended event support:

| Event | When to send |
|---|---|
| `moved` | When the flower detects movement, actuation completion, or relevant interaction. |
| `reset_requested` | When local reset, homing failure, jam detection, or operator attention is needed. |

## Robust Client Behavior

The physical device client should implement these behaviors regardless of device kind:

1. Open TCP connection to GUI host on port `9000`.
2. Send `hello` immediately after connect.
3. Start heartbeat loop, recommended every `5` seconds.
4. Start receive loop and parse newline-delimited JSON.
5. Ignore malformed JSON, unknown message types, unknown commands, and commands for other `device_id` values.
6. Clamp all numeric actuator inputs locally.
7. Put actuators into a safe stopped or neutral state if the socket disconnects.
8. Attempt reconnect with backoff, then send `hello` again after reconnect.

## Minimal Pseudocode

```text
device_id = configured stable ID
device_kind = "flower" | "anthro_sphere" | "abstract_sphere"

loop forever:
    connect TCP to gui_host:9000
    send_json_line({"type":"hello","device_id":device_id,"device_kind":device_kind})
    start heartbeat timer every 5 seconds:
        send_json_line({"type":"heartbeat","device_id":device_id})

    while socket connected:
        line = read until "\n"
        message = parse JSON object
        if message.type != "command":
            continue
        if message.device_id != device_id:
            continue

        switch message.command:
            case "set_state":
                apply_state(message.state)
            case "set_vibration":
                if device_kind == "abstract_sphere":
                    set_vibration(message.enabled)
            case "set_vibration_level":
                if device_kind == "abstract_sphere":
                    set_vibration_level(clamp(message.level, 0.0, 1.0))
            case "set_raw":
                if device_kind == "flower":
                    set_raw(message.run, clamp(message.speed), clamp(message.amplitude))

    stop actuators safely
    wait before reconnect
```

## Quick Test With GUI Fake Device

The Python fake device in `apps/gui/src/wizard_gui/fake_device.py` is the reference client shape. From `apps/gui`:

```powershell
$env:PYTHONPATH = "src;..\..\shared\python"
python run_fake_device.py --device-id abstract_01 --device-kind abstract_sphere --keyboard-events
```

For reset-request testing:

```powershell
$env:PYTHONPATH = "src;..\..\shared\python"
python run_fake_device.py --device-id abstract_01 --device-kind abstract_sphere --keyboard-events --keyboard-event-name reset_requested
```
