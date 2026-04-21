#pragma once

#include "driver/spi_master.h"
#include "driver/i2c_types.h"
#include "hal/gpio_types.h"

/*
 * Hardware configuration for the first ILI9341 display milestone.
 * Values are ESP GPIO numbers, not physical header pin numbers.
 */

#define BOARD_LCD_SPI_HOST             SPI2_HOST
#define BOARD_LCD_SPI_CLOCK_HZ         (10 * 1000 * 1000)

#define BOARD_LCD_PIN_MOSI             GPIO_NUM_11
#define BOARD_LCD_PIN_SCLK             GPIO_NUM_12
#define BOARD_LCD_PIN_CS               GPIO_NUM_10
#define BOARD_LCD_PIN_DC               GPIO_NUM_8
#define BOARD_LCD_PIN_RST              GPIO_NUM_18

/* Set to a GPIO number later if the panel has a controllable backlight pin. */
#define BOARD_LCD_PIN_BACKLIGHT        GPIO_NUM_NC
#define BOARD_LCD_BACKLIGHT_ON_LEVEL   1

#define BOARD_MPU6050_I2C_PORT         I2C_NUM_0
#define BOARD_MPU6050_PIN_SDA          GPIO_NUM_4
#define BOARD_MPU6050_PIN_SCL          GPIO_NUM_5
#define BOARD_MPU6050_I2C_CLOCK_HZ     (100 * 1000)

#define BOARD_LCD_H_RES                320
#define BOARD_LCD_V_RES                240
#define BOARD_LCD_DRAW_BUFFER_LINES    40

/* Edit these first if the picture is rotated, mirrored, color-swapped, or shifted. */
#define BOARD_LCD_COLOR_INVERT         0
#define BOARD_LCD_RGB_BGR              1
#define BOARD_LCD_MIRROR_X             0
#define BOARD_LCD_MIRROR_Y             0
#define BOARD_LCD_SWAP_XY              1
#define BOARD_LCD_SWAP_BYTES           1
#define BOARD_LCD_X_GAP                0
#define BOARD_LCD_Y_GAP                0
