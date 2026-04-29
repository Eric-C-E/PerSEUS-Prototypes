#include "device_client.h"
#include "esp_check.h"
#include "esp_log.h"
#include "flower_servo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motion_reset.h"
#include "wifi_connect.h"

static const char *TAG = "app_main";

static void motion_reset_event_cb(motion_reset_source_t source, void *user_ctx)
{
    (void)user_ctx;

    const char *source_name = (source == MOTION_RESET_SOURCE_SHAKE) ? "shake" : "touch";
    ESP_LOGI(TAG, "Motion reset requested by %s", source_name);

    esp_err_t ret = device_client_send_event("reset_requested");
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GUI not connected; reset_requested event not sent");
    } else if (ret != ESP_OK) {
        ESP_LOGW(TAG, "reset_requested event send failed: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Flower client starting");

    ESP_ERROR_CHECK(flower_servo_init());
    ESP_ERROR_CHECK(wifi_connect_start());
    ESP_ERROR_CHECK(device_client_start());
    ESP_ERROR_CHECK(motion_reset_init(motion_reset_event_cb, NULL));

    ESP_LOGI(TAG, "Flower TCP client, servo controller, and motion reset are running");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
