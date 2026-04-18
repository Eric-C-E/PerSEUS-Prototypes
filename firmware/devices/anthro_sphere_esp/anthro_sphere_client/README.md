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
