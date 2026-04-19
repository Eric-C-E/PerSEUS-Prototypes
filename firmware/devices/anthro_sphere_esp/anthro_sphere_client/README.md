# Anthro Sphere ESP Display Milestone

This ESP-IDF project brings up the first visible output for the Anthropomorphic Sphere LCD.
It initializes an SPI ILI9341 panel with `esp_lcd`, starts LVGL through `esp_lvgl_port`,
and renders a simple boot screen with a contrasting background and centered
`Anthro Sphere` label.

No animation, touch, networking, filesystem, image assets, or higher-level application
logic is included in this milestone.

## Hardware Assumptions

- LCD controller: ILI9341
- Resolution: 320x240
- Interface: SPI
- Driver component: `espressif/esp_lcd_ili9341`
- LVGL integration: `espressif/esp_lvgl_port`
- Backlight GPIO: not configured by default; assumed externally powered

All pins below are ESP GPIO numbers, not physical header pin numbers.

| LCD signal | ESP GPIO |
| ---------- | -------- |
| MOSI       | GPIO11   |
| SCLK       | GPIO12   |
| CS         | GPIO10   |
| DC         | GPIO8    |
| RST        | GPIO18   |

MISO is not configured for this write-only display path.

## Display Tuning

Edit [main/board_config.h](main/board_config.h) first if the display is blank,
mirrored, rotated, shifted, or color-swapped.

Start with:

- `BOARD_LCD_SPI_HOST`
- `BOARD_LCD_SPI_CLOCK_HZ`
- `BOARD_LCD_COLOR_INVERT`
- `BOARD_LCD_RGB_BGR`
- `BOARD_LCD_MIRROR_X`
- `BOARD_LCD_MIRROR_Y`
- `BOARD_LCD_SWAP_XY`
- `BOARD_LCD_SWAP_BYTES`
- `BOARD_LCD_X_GAP`
- `BOARD_LCD_Y_GAP`
- `BOARD_LCD_PIN_BACKLIGHT`

If the panel has a backlight control wire, set `BOARD_LCD_PIN_BACKLIGHT` to that
GPIO and adjust `BOARD_LCD_BACKLIGHT_ON_LEVEL` if needed. By default it is
`GPIO_NUM_NC`, so the firmware does not drive a backlight pin.

## Build, Flash, Monitor

From this project root:

```powershell
idf.py build
idf.py flash
idf.py monitor
```

Or specify the port:

```powershell
idf.py -p COM3 build flash monitor
```

Press `Ctrl+]` to exit the monitor.

## Face Renderer Milestone

The project now has a pre-drawn-frame face renderer layered on top of LVGL:

- `main/face_renderer.c/.h` owns one `lv_image` widget and advances frame arrays
  at a low fixed frame rate.
- `main/face_fsm.c/.h` owns a primitive state machine with five face states,
  blink timing, and neutral-routed transitions.
- `main/face_assets.h` and `main/face_assets.c` register animation frame sets in
  one place.
- `main/assets/faces/generated/` contains buildable LVGL C image assets.

The current source images in `assets/faces/src/` have been converted into
compiled LVGL image descriptors under `main/assets/faces/generated/`.

At boot the renderer shows `loading_screen` for one second, then enters
`neutral_idle`. Blinks happen every 3 seconds while the FSM is idle.

Transitions always route through neutral. For example:

```text
low_neg -> high_pos
low_neg_to_neutral using neutral_to_low_neg
neutral_idle
neutral_to_high_pos
high_pos_idle
```

If a blink is active when a state command arrives, the blink is ignored and the
state transition starts immediately.

### Expected frame asset format and placement

Author source art as:

- PNG
- 320x240 pixels
- one full-screen frame per image
- no runtime filesystem dependency

`loading_screen.png` is converted as `LV_COLOR_FORMAT_RGB565`.

All face frames are converted as black-on-white monochrome:

- `LV_COLOR_FORMAT_I1`
- source value 0 means white
- source HIGH/nonzero/dark line pixels mean black

Place source PNGs under:

```text
assets/faces/src/
```

Place converted LVGL C image assets under:

```text
main/assets/faces/generated/
```

Use LVGL 9 image descriptors exported as `const lv_image_dsc_t` symbols. The
firmware uses `lv_image` widgets and compiled-in C assets only.

The required monochrome convention is:

- source value `0` -> white pixel
- source HIGH/nonzero -> black pixel
- generated LVGL format: `LV_COLOR_FORMAT_I1`
- palette index 0: white
- palette index 1: black

If the converter emits the opposite polarity, invert the source image or the
generated bit data before registering the frame. Do this before the frame is
handed to LVGL; the renderer assumes registered frames already follow the
black/white convention.

### How to add your drawings

1. Add source PNGs to `assets/faces/src/`.
2. Convert `loading_screen.png` to RGB565.
3. Convert all face frames to I1, white index 0, black index 1.
4. Put generated `.c` files in `main/assets/faces/generated/`.
5. Make sure `main/face_assets.h` declares each generated symbol.
6. Make sure `main/face_assets.c` registers the symbols in the correct state.

Required source names:

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
