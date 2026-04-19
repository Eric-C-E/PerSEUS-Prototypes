#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t ui_boot_create(void);
lv_obj_t *ui_boot_get_root(void);
