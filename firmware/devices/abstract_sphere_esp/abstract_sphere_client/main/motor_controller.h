#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t motor_controller_init(void);
esp_err_t motor_controller_set_enabled(bool enabled);
esp_err_t motor_controller_set_level(float level);
esp_err_t motor_controller_stop(void);