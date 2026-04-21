#pragma once

#include "esp_err.h"

typedef enum {
    LIGHTING_STATE_NEUTRAL = 0,
    LIGHTING_STATE_LOW_NEGATIVE,
    LIGHTING_STATE_LOW_POSITIVE,
    LIGHTING_STATE_HIGH_NEGATIVE,
    LIGHTING_STATE_HIGH_POSITIVE,
    LIGHTING_STATE_COUNT,
} lighting_state_t;

esp_err_t lighting_controller_init(void);
esp_err_t lighting_controller_set_state(lighting_state_t state);
const char *lighting_controller_state_name(lighting_state_t state);