#include "device_client.h"
#include "display_init.h"
#include "esp_check.h"
#include "esp_log.h"
#include "face_assets.h"
#include "face_fsm.h"
#include "face_renderer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motion_reset.h"
#include "ui_boot.h"
#include "wifi_connect.h"

static const char *TAG = "app_main";

static void motion_reset_event_cb(motion_reset_source_t source, void *user_ctx)
{
    (void)user_ctx;

    const char *source_name = (source == MOTION_RESET_SOURCE_SHAKE) ? "shake" : "touch";
    ESP_LOGI(TAG, "Motion reset requested by %s", source_name);
    ESP_ERROR_CHECK_WITHOUT_ABORT(device_client_send_event("reset_requested"));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Anthro Sphere display milestone starting");

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(ui_boot_create());
    ESP_ERROR_CHECK(face_renderer_init(ui_boot_get_root()));
    ESP_ERROR_CHECK(face_renderer_play_animation(&g_face_anim_loading_screen));
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_ERROR_CHECK(face_fsm_init());
    ESP_ERROR_CHECK(motion_reset_init(motion_reset_event_cb, NULL));
    ESP_ERROR_CHECK(wifi_connect_start());
    ESP_ERROR_CHECK(device_client_start());

    ESP_LOGI(TAG, "Face renderer, motion reset skeleton, and Anthro Sphere TCP client are running");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
