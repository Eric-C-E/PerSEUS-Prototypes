# Anthro Sphere Client Agent Context

This is an existing ESP-IDF project for the Anthropomorphic Sphere LCD client.
Do not recreate the project or replace it with a new ESP-IDF skeleton.

## Current Hardware Baseline

- Target currently configured in `sdkconfig`: ESP32-S3.
- LCD controller: ILI9341.
- Display interface: SPI.
- Resolution: 320x240 landscape.
- Display driver stack: `esp_lcd` + `espressif/esp_lcd_ili9341`.
- LVGL integration: `espressif/esp_lvgl_port`.

Known-good LCD settings are in `main/board_config.h`:

- MOSI: GPIO11
- SCLK: GPIO12
- CS: GPIO10
- DC: GPIO8
- RST: GPIO18
- SPI clock: 10 MHz
- `BOARD_LCD_SWAP_XY`: 1
- `BOARD_LCD_SWAP_BYTES`: 1
- `BOARD_LCD_RGB_BGR`: 1
- `BOARD_LCD_MIRROR_X`: 0
- `BOARD_LCD_MIRROR_Y`: 0

The 10 MHz SPI clock currently gives a visually pleasing animated scan/update feel.
Do not raise it unless there is a concrete performance reason.

## Face Rendering

The face renderer is intentionally frame-asset based, not procedural.

Important files:

- `main/face_renderer.c/.h`: owns one LVGL image widget and plays frame arrays.
- `main/face_fsm.c/.h`: controls state transitions, blink timing, and neutral routing.
- `main/face_assets.c/.h`: declares and registers frame assets.
- `main/assets/faces/generated/`: compiled LVGL C image descriptors.
- `assets/faces/src/`: source PNG frames.

Animation rate:

- `FACE_DEFAULT_FRAME_TIME_MS` is 125 ms, so animations play at 8 FPS.
- Blinks occur every `FACE_BLINK_PERIOD_MS`, currently 3000 ms.

The local demo state cycle is disabled:

```c
#define FACE_FSM_ENABLE_DEMO_CYCLE 0
```

This means GUI `set_state` commands drive emotional state. Keep local blinking enabled.

## Face States And Transitions

Supported states:

- `neutral`
- `low_neg`
- `low_pos`
- `high_neg`
- `high_pos`

The GUI sends:

- `neutral`
- `low_negative`
- `low_positive`
- `high_negative`
- `high_positive`

These map to:

- `FACE_STATE_NEUTRAL`
- `FACE_STATE_LOW_NEG`
- `FACE_STATE_LOW_POS`
- `FACE_STATE_HIGH_NEG`
- `FACE_STATE_HIGH_POS`

Transitions always route through neutral. Example:

```text
low_neg -> high_pos
low_neg_to_neutral using neutral_to_low_neg
neutral_idle
neutral_to_high_pos
high_pos_idle
```

If a blink is active when a GUI state command arrives, the blink is ignored and
the state transition starts immediately.

## Asset Format

`loading_screen.png` is RGB565.

All face frames are 1-bit indexed monochrome:

- LVGL format: `LV_COLOR_FORMAT_I1`
- 320x240
- palette index 0: white
- palette index 1: black
- source/background white resolves to 0
- black face lines resolve to 1 / HIGH / nonzero

Current required source file names:

- `loading_screen.png`
- `neutral_idle.png`
- `neutral_blink.png`
- `low_neg_idle.png`
- `low_neg_blink.png`
- `low_pos_idle.png`
- `low_pos_blink.png`
- `high_neg_idle.png`
- `high_neg_blink.png`
- `high_pos_idle.png`
- `high_pos_blink.png`
- `neutral_to_low_neg.png`
- `neutral_to_low_pos.png`
- `neutral_to_high_neg.png`
- `neutral_to_high_pos.png`

Generated symbols use the `face_` prefix, for example `neutral_idle.png`
becomes `face_neutral_idle`.

## Wizard GUI TCP Client

This firmware implements the Anthro Sphere section of `device_client_interface.md`.

Important files:

- `main/client_config.h`: Wi-Fi and GUI connection config.
- `main/wifi_connect.c/.h`: Wi-Fi station setup.
- `main/device_client.c/.h`: TCP client, hello/heartbeat, JSON command parsing.

Device registration:

```json
{"type":"hello","device_id":"anthro_01","device_kind":"anthro_sphere"}
```

Supported command:

- `set_state`

Unsupported commands are ignored. Do not add abstract-sphere vibration or flower
raw motor behavior to this firmware unless explicitly requested.

Before flashing on a new network, edit:

- `DEVICE_CLIENT_GUI_HOST`
- `DEVICE_WIFI_SSID`
- `DEVICE_WIFI_PASSWORD`

in `main/client_config.h`.

## Motion Reset / Future MPU6050

`main/motion_reset.c/.h` is a reusable skeleton for future MPU6050-based
shake/touch detection.

The MPU6050 driver is not implemented yet. The intended integration point is:

```c
motion_reset_raise_shake_flag();
```

When that function is called, the firmware sends:

```json
{"type":"event","device_id":"anthro_01","event":"reset_requested"}
```

There is also a reserved touch hook:

```c
motion_reset_raise_touch_flag();
```

Keep the eventual shake detector binary: if shake is detected, raise the flag.
Do not make the GUI protocol depend directly on MPU6050 details.

## Build Notes

Use an ESP-IDF-enabled terminal. This environment may not have `idf.py` on PATH
inside generic shells.

If Windows path-length errors appear, build with a short build directory:

```powershell
idf.py -B C:\espbuild\anthro_sphere_client build flash monitor
```

Managed components are used through `main/idf_component.yml`.
