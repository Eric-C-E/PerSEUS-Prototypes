#include "ui_boot.h"

#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "ui_boot";

void ui_boot_show(lv_disp_t *display)
{
    ESP_LOGI(TAG, "Create boot LCD test screen");

    lvgl_port_lock(0);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *screen = lv_display_get_screen_active(display);
#else
    lv_obj_t *screen = lv_disp_get_scr_act(display);
#endif

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x004A7C), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Anthropomorphic\nSphere");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
#endif
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    /*
     * Later frame-based face animation can replace this boot screen after the
     * LCD/LVGL path is proven stable.
     */

    lvgl_port_unlock();

    ESP_LOGI(TAG, "Boot LCD test screen submitted");
}
