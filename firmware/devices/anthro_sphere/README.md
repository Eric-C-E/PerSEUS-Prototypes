# Anthropomorphic Sphere LCD Milestone

This is the first bottom-up firmware milestone for the `anthropomorphic_sphere`
ESP-IDF project. On boot it initializes a SPI LCD panel through Espressif's
`esp_lcd` stack, registers it with LVGL through `esp_lvgl_port`, turns on the
backlight, and shows a simple centered boot label.

## Current Assumptions

- Target display resolution is 320x240.
- The LCD is connected over SPI.
- The default panel controller is ILI9341 via `espressif/esp_lcd_ili9341`.
- Touch, image loading, animation, Wi-Fi, and networking are intentionally out
  of scope for this milestone.
- The default GPIOs are placeholders and must be checked against the real board.

## Edit These Values First

All board-specific values live in `main/board_config.h`.

- `BOARD_LCD_SPI_HOST`
- `BOARD_LCD_PIN_NUM_SCLK`
- `BOARD_LCD_PIN_NUM_MOSI`
- `BOARD_LCD_PIN_NUM_MISO`
- `BOARD_LCD_PIN_NUM_CS`
- `BOARD_LCD_PIN_NUM_DC`
- `BOARD_LCD_PIN_NUM_RST`
- `BOARD_LCD_PIN_NUM_BK_LIGHT`
- `BOARD_LCD_BACKLIGHT_ON_LEVEL`
- `BOARD_LCD_BACKLIGHT_OFF_LEVEL`
- `BOARD_LCD_COLOR_INVERT`
- `BOARD_LCD_BGR_ORDER`
- `BOARD_LCD_SWAP_XY`
- `BOARD_LCD_MIRROR_X`
- `BOARD_LCD_MIRROR_Y`
- `BOARD_LCD_SWAP_COLOR_BYTES`
- `BOARD_LCD_X_GAP`
- `BOARD_LCD_Y_GAP`

If the actual controller is not ILI9341-compatible, change the component
dependency in `main/idf_component.yml` and switch the constructor in
`main/display_init.c` where `esp_lcd_new_panel_ili9341()` is called.

## Build, Flash, Monitor

From this project directory:

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

Use a different target in `idf.py set-target` if the board is ESP32-S3, ESP32-C3,
or another ESP32-family chip.

## File Layout

- `main/app_main.c`: application entry point only.
- `main/display_init.c`: backlight, SPI bus, panel IO, panel driver, and LVGL
  display registration.
- `main/board_config.h`: all board and panel configuration switches.
- `main/ui_boot.c`: the minimal visible LVGL boot UI.
- `main/idf_component.yml`: ESP-IDF component manager dependencies.

When the display path is proven stable, the future frame-based face animation
can replace or load after `ui_boot_show()`.
