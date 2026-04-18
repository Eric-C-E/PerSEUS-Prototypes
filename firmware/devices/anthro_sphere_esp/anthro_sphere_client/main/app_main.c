#include "display_init.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_boot.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "Anthro Sphere display milestone starting");

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(ui_boot_create());

    ESP_LOGI(TAG, "Boot UI is running");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
