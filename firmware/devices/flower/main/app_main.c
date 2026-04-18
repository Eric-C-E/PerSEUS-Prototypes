#include "perseus_common.h"

#include "esp_log.h"

static const char *TAG = "flower";

void app_main(void)
{
    perseus_common_init(PERSEUS_DEVICE_FLOWER);
    ESP_LOGI(TAG, "Flower firmware scaffold is ready");
}
