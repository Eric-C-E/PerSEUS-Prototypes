#include "perseus_common.h"

#include "esp_log.h"

static const char *TAG = "abstract_sphere";

void app_main(void)
{
    perseus_common_init(PERSEUS_DEVICE_ABSTRACT_SPHERE);
    ESP_LOGI(TAG, "Abstract sphere firmware scaffold is ready");
}
