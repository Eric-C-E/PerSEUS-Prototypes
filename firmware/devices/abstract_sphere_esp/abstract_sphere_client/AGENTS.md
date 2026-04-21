# Abstract Sphere Client Notes

This firmware is the ESP32-S3 `abstract_sphere_client`, a variant of the working `anthro_sphere_client` under `firmware/devices/anthro_sphere_esp/anthro_sphere_client`.

## Protocol

The Wizard-of-Oz GUI is the TCP server on port `9000`. The device is a TCP client using newline-delimited UTF-8 JSON. The local protocol contract is documented in `main/device_client_interface.md`.

This client registers as:

- `device_id`: `abstract_01`
- `device_kind`: `abstract_sphere`

Implemented commands:

- `set_state`: maps emotional states to the lighting placeholder.
- `set_vibration`: enables/disables vibration immediately.
- `set_vibration_level`: clamps GUI level locally and applies it through the motor safety envelope.

The MPU6050 shake detector is a shared feature across device classes and sends `reset_requested` events.

## Hardware Pins

Current board assumptions live in `main/board_config.h`.

- SPI2 reserved pins:
  - MOSI `GPIO11`
  - MISO `GPIO13`
  - SCLK `GPIO12`
  - CS `GPIO10`
- MPU6050 I2C:
  - SDA `GPIO4`
  - SCL `GPIO5`
- DRV8833 vibration motor input:
  - ESP32-S3 `GPIO16` -> DRV8833 `IN1`
  - DRV8833 `IN2` is expected to be tied low for one-direction vibration.
  - Motor connects directly to DRV8833 `OUT1` / `OUT2`.
- LED ring is not connected yet. `BOARD_LED_RING_PIN` is `GPIO_NUM_NC` and `lighting_controller.c` only logs the future pattern.

## Motor Safety

`main/motor_controller.c` intentionally uses conservative defaults because the vibration motor's safe operating current is not yet characterized.

Current policy:

- Startup state is stopped.
- Default requested level is `20%`.
- Absolute PWM cap is `30%`, even if the GUI sends `level: 1.0`.
- Continuous vibration is auto-stopped after `2000 ms`.
- Socket disconnect stops vibration immediately.

Do not remove these limits without confirming the motor, supply, and DRV8833 thermal/current behavior on hardware.

## Lighting Placeholder

`main/lighting_controller.c` replaces the anthro face renderer/FSM for this device class. It currently logs the state-to-pattern mapping that a future LED ring driver should execute. Keep command handling in place even before the LED hardware is connected so GUI integration can be tested via logs.

## Source Relationship

Reusable code copied/adapted from `anthro_sphere_client`:

- `device_client` TCP framing/reconnect/heartbeat shape
- `wifi_connect`
- `motion_reset` MPU6050 shake detector

Face/display/LVGL/ILI9341 code from `anthro_sphere_client` is intentionally not used in this project.

## Build Policy

Do not build after changes unless the user explicitly asks. The user will run ESP-IDF builds locally.

If asked to build, this project expects ESP-IDF v6.0 for target `esp32s3`; use the user's configured ESP-IDF shell/environment rather than modifying system policy or installing tools unless explicitly requested.