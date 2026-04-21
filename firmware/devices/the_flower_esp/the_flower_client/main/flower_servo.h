#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    FLOWER_STATE_NEUTRAL = 0,
    FLOWER_STATE_HIGH_NEGATIVE,
    FLOWER_STATE_HIGH_POSITIVE,
    FLOWER_STATE_LOW_NEGATIVE,
    FLOWER_STATE_LOW_POSITIVE,
} flower_state_t;

esp_err_t flower_servo_init(void);
esp_err_t flower_servo_set_state(flower_state_t state);
esp_err_t flower_servo_set_raw(bool run, float speed, float amplitude);
esp_err_t flower_servo_set_tilt(float tilt);
esp_err_t flower_servo_stop(void);
