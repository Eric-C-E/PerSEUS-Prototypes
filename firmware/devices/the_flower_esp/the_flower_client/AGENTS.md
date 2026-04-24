# Flower Client Agent Context

This is an ESP-IDF project for the PerSEUS Flower physical client. Do not
replace it with a fresh ESP-IDF skeleton.

The Flower client was intentionally patterned after:

```text
firmware/devices/anthro_sphere_esp/anthro_sphere_client
```

Reuse that project as the reference for Wi-Fi setup, TCP connection behavior,
newline-delimited JSON parsing, hello/heartbeat framing, reconnect policy, and
general device-client robustness. Do not copy its LCD, LVGL, face renderer, face
FSM, face assets, or SPI display code into this project.

## Current Hardware Baseline

- Target currently configured in `sdkconfig`: ESP32-S3.
- Actuators: two MG995 servomotors.
- Servo PWM implementation: ESP-IDF LEDC at 50 Hz.
- Servo axes:
  - Tilt servo: `BOARD_SERVO_TILT_GPIO`, currently GPIO6.
  - Rotation servo: `BOARD_SERVO_ROTATION_GPIO`, currently GPIO7.

Servo pin, pulse, direction, and angle tuning lives in `main/board_config.h`.
Expect these values to change during hardware testing:

- `BOARD_SERVO_MIN_PULSE_US`
- `BOARD_SERVO_MAX_PULSE_US`
- `BOARD_SERVO_TILT_MIN_DEG`
- `BOARD_SERVO_TILT_CENTER_DEG`
- `BOARD_SERVO_TILT_MAX_DEG`
- `BOARD_SERVO_TILT_REVERSED`
- `BOARD_SERVO_ROTATION_MIN_DEG`
- `BOARD_SERVO_ROTATION_CENTER_DEG`
- `BOARD_SERVO_ROTATION_MAX_DEG`
- `BOARD_SERVO_ROTATION_REVERSED`
- `BOARD_SERVO_RAW_MIN_HZ`
- `BOARD_SERVO_RAW_MAX_HZ`

Be conservative with servo limits. The GUI may send out-of-range numeric values;
firmware must clamp before driving actuators.

Recent servo calibration experiment:

- The standalone calibrator was run against the current Flower mechanism.
- Both MG995 servos behaved like standard positional servos, not continuous
  rotation servos.
- `1500 us` behaved as the practical neutral/center pulse.
- The tested usable travel was close to 170 degrees without obvious binding.
- Current production values were updated to the calibrator's full shared range:
  `BOARD_SERVO_MIN_PULSE_US=500`, `BOARD_SERVO_MAX_PULSE_US=2500`, tilt
  `0/90/180`, rotation `0/90/180`, and both reversal flags `0`.
- If dashboard poses or raw oscillation produce buzzing near the extremes,
  narrow the angle range or repeat calibration with a larger margin, for
  example `5.0f` to `175.0f`.

## Wizard GUI TCP Client

This firmware implements the Flower section of `main/device_client_interface.md`.
Always reread that file before changing command parsing because the GUI protocol
is still evolving.

Important files:

- `main/client_config.h`: Wi-Fi and GUI connection config.
- `main/wifi_connect.c/.h`: Wi-Fi station setup, ported from anthro_sphere.
- `main/device_client.c/.h`: TCP client, hello/heartbeat, JSON command parsing.
- `main/flower_servo.c/.h`: servo control and command behavior.
- `main/motion_reset.c/.h`: shared MPU6050 shake-to-reset-request feature.
- `main/board_config.h`: hardware and tuning constants.

Device registration:

```json
{"type":"hello","device_id":"flower_01","device_kind":"flower"}
```

Before flashing on a new network, edit these in `main/client_config.h`:

- `DEVICE_CLIENT_GUI_HOST`
- `DEVICE_WIFI_SSID`
- `DEVICE_WIFI_PASSWORD`

## Supported Flower Commands

The client should ignore malformed JSON, unknown commands, and commands for
other `device_id` values without crashing.

### `set_state`

Payload:

```json
{"type":"command","device_id":"flower_01","command":"set_state","state":"high_positive"}
```

Accepted state strings:

- `neutral`
- `high_negative`
- `high_positive`
- `low_negative`
- `low_positive`

These map to local static poses in `state_to_pose()` in `main/flower_servo.c`.
The exact pose angles are provisional and should be tuned against the physical
Flower mechanism.

Current policy:

- `set_state` updates the target tilt and target rotation.
- Tilt is applied immediately.
- Rotation is applied immediately only when raw rotation mode is not active.
- If raw rotation mode is active, the new target rotation becomes the center
  angle for the raw oscillation.

### `set_raw`

Payload:

```json
{"type":"command","device_id":"flower_01","command":"set_raw","run":true,"speed":0.5,"amplitude":0.8}
```

Current protocol meaning: raw flower rotation control only.

Current policy:

- Clamp `speed` and `amplitude` to `[0.0, 1.0]`.
- When `run` is true, oscillate only the rotation servo.
- Raw rotation oscillates around `target_rotation_deg`.
- Tilt is not moved by raw rotation.
- When `run` is false, stop oscillation and return the rotation servo to
  `target_rotation_deg`.

### `set_tilt`

Payload:

```json
{"type":"command","device_id":"flower_01","command":"set_tilt","tilt":0.5}
```

Current policy:

- Clamp `tilt` to `[0.0, 1.0]`.
- Map normalized `tilt` to `BOARD_SERVO_TILT_MIN_DEG` through
  `BOARD_SERVO_TILT_MAX_DEG`.
- Apply tilt immediately, independent of raw rotation mode.

## Safe Behavior

On GUI socket disconnect, `device_client.c` calls `flower_servo_stop()`. Current
stop behavior:

- Raw rotation is disabled.
- Stored state resets to neutral.
- Target tilt and target rotation reset to their configured centers.
- Both servos are driven to the neutral pose.

Keep this conservative unless the physical testing shows a different safe pose
is required.

## Motion Reset / MPU6050

The Flower includes the shared MPU6050 shake-to-request-reset feature from the
anthro_sphere client.

Current IMU config in `main/board_config.h`:

- SDA: GPIO4
- SCL: GPIO5
- I2C port: `I2C_NUM_0`
- I2C clock: 100 kHz

`main/app_main.c` starts `motion_reset_init()` after `device_client_start()`.
When `motion_reset.c` detects a shake, it calls the app callback, which sends:

```json
{"type":"event","device_id":"flower_01","event":"reset_requested"}
```

If the MPU6050 is absent or wired incorrectly, the task logs retry warnings and
keeps retrying; the rest of the Flower client should continue running.

## Build Notes

Use an ESP-IDF-enabled terminal. The generic shell available to Codex may not
have `idf.py` on PATH. The user currently intends to build and report hardware
or compile feedback, so do not spend time trying to build unless explicitly
asked.

If Windows path-length errors appear, use a short build directory, for example:

```powershell
idf.py -B C:\espbuild\the_flower_client build flash monitor
```

## Accessory Calibration Tool

There is a standalone servo calibration app in `tools/servo_calibrator`. It is
not referenced by the Flower client build and should remain separate from the
production firmware.

Use it to find safe servo pulse limits, center positions, and reversal flags.
It prints serial-console instructions at runtime and can emit ready-to-paste
`main/board_config.h` values.

During the latest test, the original calibrator `fgets(stdin)` loop repeatedly
printed `cal>` and did not accept input on the user's ESP32-S3 monitor setup.
The calibrator was changed to install the UART driver and read commands with
`uart_read_bytes()` directly, with local echo/backspace handling. Keep that
direct-UART input path unless a later ESP-IDF console configuration proves a
cleaner interactive input path on this board.

Current production firmware uses one shared pulse range for both axes, so the
calibrator prints the safe overlapping pulse range. If physical testing shows
the tilt and rotation servos need substantially different pulse limits, update
`main/flower_servo.c` to support per-axis pulse min/max before applying tight
per-axis limits.

Safe calibration practice:

- Do not intentionally bind the mechanism to discover limits.
- Approach endpoints in small pulse steps, stop when motion slows, buzzing
  increases, or the linkage approaches a hard stop, then back off before
  marking `min` or `max`.
- MG995 servo position is controlled mainly by pulse width at 50 Hz; typical
  values are roughly `1000 us` one side, `1500 us` center, and `2000 us` the
  other side, although these units accepted `500-2500 us` in the current
  mechanism.
