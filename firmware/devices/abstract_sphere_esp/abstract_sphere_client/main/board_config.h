#pragma once

#include "driver/i2c_types.h"
#include "hal/gpio_types.h"

/*
 * Abstract Sphere hardware configuration.
 * Values are ESP GPIO numbers, not physical header pin numbers.
 */

/* Reserved SPI2 pins for future peripherals on the abstract sphere hardware. */
#define BOARD_SPI_PIN_MOSI              GPIO_NUM_11
#define BOARD_SPI_PIN_MISO              GPIO_NUM_13
#define BOARD_SPI_PIN_SCLK              GPIO_NUM_12
#define BOARD_SPI_PIN_CS                GPIO_NUM_10

#define BOARD_MPU6050_I2C_PORT          I2C_NUM_0
#define BOARD_MPU6050_PIN_SDA           GPIO_NUM_4
#define BOARD_MPU6050_PIN_SCL           GPIO_NUM_5
#define BOARD_MPU6050_I2C_CLOCK_HZ      (100 * 1000)

/* DRV8833: GPIO16 -> IN1, IN2 tied low for one-direction vibration. */
#define BOARD_VIBE_MOTOR_PWM_PIN        GPIO_NUM_16

/* WS2812B-compatible 24-LED ring. GPIO drives ring DI; DO is unused. */
#define BOARD_LED_RING_PIN              GPIO_NUM_17
#define BOARD_LED_RING_COUNT            24
#define BOARD_LED_RING_BRIGHTNESS_PERCENT 8
