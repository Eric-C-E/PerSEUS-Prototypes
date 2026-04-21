#pragma once

#include "esp_err.h"

typedef enum {
    MOTION_RESET_SOURCE_SHAKE = 0,
    MOTION_RESET_SOURCE_TOUCH,
} motion_reset_source_t;

typedef void (*motion_reset_callback_t)(motion_reset_source_t source, void *user_ctx);

esp_err_t motion_reset_init(motion_reset_callback_t callback, void *user_ctx);

/*
 * Hook for MPU6050 shake detection or a test/manual trigger.
 * Calling this schedules a reset-request callback from the motion reset task.
 */
void motion_reset_raise_shake_flag(void);

/*
 * Reserved hook for future touch-like reset gestures if needed.
 */
void motion_reset_raise_touch_flag(void);
