#include "device_client.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lighting_controller.h"
#include "motion_reset.h"
#include "motor_controller.h"
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
    ESP_LOGI(TAG, "Abstract Sphere client starting");

    ESP_ERROR_CHECK(lighting_controller_init());
    ESP_ERROR_CHECK(motor_controller_init());
    ESP_ERROR_CHECK(motion_reset_init(motion_reset_event_cb, NULL));
    ESP_ERROR_CHECK(wifi_connect_start());
    ESP_ERROR_CHECK(device_client_start());

    ESP_LOGI(TAG, "Abstract Sphere TCP client, lighting placeholder, vibration motor, and MPU6050 reset detection are running");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}