#include "display_init.h"
#include "esp_check.h"
#include "esp_log.h"
#include "face_assets.h"
#include "face_fsm.h"
#include "face_renderer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_boot.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "Anthro Sphere display milestone starting");

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(ui_boot_create());
    ESP_ERROR_CHECK(face_renderer_init(ui_boot_get_root()));
    ESP_ERROR_CHECK(face_renderer_play_animation(&g_face_anim_loading_screen));
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_ERROR_CHECK(face_fsm_init());

    ESP_LOGI(TAG, "Face renderer and primitive FSM are running");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
