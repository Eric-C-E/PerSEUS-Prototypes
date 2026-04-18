#include "ui_boot.h"

#include "display_init.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "ui_boot";

esp_err_t ui_boot_create(void)
{
    ESP_LOGI(TAG, "UI creation");

    lv_display_t *display = display_init_get_lvgl_display();
    ESP_RETURN_ON_FALSE(display, ESP_ERR_INVALID_STATE, TAG, "LVGL display is not initialized");

    ESP_RETURN_ON_FALSE(lvgl_port_lock(0), ESP_ERR_TIMEOUT, TAG, "LVGL lock failed");

    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0047AB), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Anthro Sphere");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(label, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(label, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(label, 8, LV_PART_MAIN);
    lv_obj_center(label);

    lvgl_port_unlock();

    return ESP_OK;
}
