#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

/*
 * First hardware bring-up defaults.
 *
 * TODO: Replace these values with the exact wiring for the target board before
 * flashing. The code assumes a 320x240 SPI ILI9341-compatible panel.
 */

#define BOARD_LCD_H_RES                  (320)
#define BOARD_LCD_V_RES                  (240)
#define BOARD_LCD_X_GAP                  (0)
#define BOARD_LCD_Y_GAP                  (0)

#define BOARD_LCD_SPI_HOST               (SPI2_HOST)
#define BOARD_LCD_PIXEL_CLOCK_HZ         (20 * 1000 * 1000)

#define BOARD_LCD_PIN_NUM_SCLK           (GPIO_NUM_18)
#define BOARD_LCD_PIN_NUM_MOSI           (GPIO_NUM_23)
#define BOARD_LCD_PIN_NUM_MISO           (GPIO_NUM_NC)
#define BOARD_LCD_PIN_NUM_CS             (GPIO_NUM_5)
#define BOARD_LCD_PIN_NUM_DC             (GPIO_NUM_2)
#define BOARD_LCD_PIN_NUM_RST            (GPIO_NUM_4)
#define BOARD_LCD_PIN_NUM_BK_LIGHT       (GPIO_NUM_15)

#define BOARD_LCD_BACKLIGHT_ON_LEVEL     (1)
#define BOARD_LCD_BACKLIGHT_OFF_LEVEL    (0)

/*
 * Tune these if the image appears rotated, mirrored, color-swapped, or washed
 * out. ILI9341 breakout boards commonly differ here.
 */
#define BOARD_LCD_SWAP_XY                (false)
#define BOARD_LCD_MIRROR_X               (false)
#define BOARD_LCD_MIRROR_Y               (false)
#define BOARD_LCD_COLOR_INVERT           (false)
#define BOARD_LCD_BGR_ORDER              (false)
#define BOARD_LCD_SWAP_COLOR_BYTES       (false)

/* Partial draw buffer: 320 x 40 x RGB565 x two buffers ~= 50 KiB DMA RAM. */
#define BOARD_LCD_DRAW_BUFFER_LINES      (40)
