#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t display_init(void);
lv_display_t *display_init_get_lvgl_display(void);
