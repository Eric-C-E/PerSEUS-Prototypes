#pragma once

#include "driver/i2c_types.h"
#include "hal/gpio_types.h"

/*
 * Flower actuator configuration.
 *
 * MG995 servos usually accept a 50 Hz signal with roughly 500-2500 us pulses,
 * but real units and linkages vary. Keep all tuning here while finding the
 * physical min/max angles and any reversed-axis quirks.
 */

#define BOARD_SERVO_TILT_GPIO             GPIO_NUM_6
#define BOARD_SERVO_ROTATION_GPIO         GPIO_NUM_7

#define BOARD_SERVO_PWM_FREQ_HZ           50
#define BOARD_SERVO_PWM_RES_BITS          14

#define BOARD_SERVO_MIN_PULSE_US          500
#define BOARD_SERVO_MAX_PULSE_US          2500
#define BOARD_SERVO_PERIOD_US             20000

#define BOARD_SERVO_TILT_MIN_DEG          35.0f
#define BOARD_SERVO_TILT_CENTER_DEG       90.0f
#define BOARD_SERVO_TILT_MAX_DEG          145.0f
#define BOARD_SERVO_TILT_REVERSED         0

#define BOARD_SERVO_ROTATION_MIN_DEG      20.0f
#define BOARD_SERVO_ROTATION_CENTER_DEG   90.0f
#define BOARD_SERVO_ROTATION_MAX_DEG      160.0f
#define BOARD_SERVO_ROTATION_REVERSED     0

#define BOARD_SERVO_UPDATE_PERIOD_MS      20
#define BOARD_SERVO_RAW_MIN_HZ            0.10f
#define BOARD_SERVO_RAW_MAX_HZ            1.50f

/*
 * Shared motion-reset IMU configuration.
 * The MPU6050 shake detector sends reset-request events to the Wizard GUI.
 */

#define BOARD_MPU6050_I2C_PORT            I2C_NUM_0
#define BOARD_MPU6050_PIN_SDA             GPIO_NUM_4
#define BOARD_MPU6050_PIN_SCL             GPIO_NUM_5
#define BOARD_MPU6050_I2C_CLOCK_HZ        (100 * 1000)
