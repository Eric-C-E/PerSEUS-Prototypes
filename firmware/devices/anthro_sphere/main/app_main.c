#include "esp_log.h"

#include "display_init.h"
#include "ui_boot.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Anthropomorphic Sphere LCD milestone");

    lv_disp_t *display = display_init();
    ui_boot_show(display);

    ESP_LOGI(TAG, "LCD milestone UI is running");
}
