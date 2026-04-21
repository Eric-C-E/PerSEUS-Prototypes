#pragma once

#include "esp_err.h"

typedef enum {
    MOTION_RESET_SOURCE_SHAKE = 0,
    MOTION_RESET_SOURCE_TOUCH,
} motion_reset_source_t;

typedef void (*motion_reset_callback_t)(motion_reset_source_t source, void *user_ctx);

esp_err_t motion_reset_init(motion_reset_callback_t callback, void *user_ctx);

/*
 * Temporary hook for the future MPU6050 shake detector.
 * Call this from the final binary shake-detection code when a shake is detected.
 */
void motion_reset_raise_shake_flag(void);

/*
 * Temporary hook for future touch-like motion detection if needed by another device.
 */
void motion_reset_raise_touch_flag(void);
