#include "perseus_common.h"

#include "esp_log.h"

static const char *TAG = "perseus_common";

const char *perseus_device_type_name(perseus_device_type_t device_type)
{
    switch (device_type) {
    case PERSEUS_DEVICE_ANTHRO_SPHERE:
        return "anthro_sphere";
    case PERSEUS_DEVICE_ABSTRACT_SPHERE:
        return "abstract_sphere";
    case PERSEUS_DEVICE_FLOWER:
        return "flower";
    default:
        return "unknown";
    }
}

void perseus_common_init(perseus_device_type_t device_type)
{
    ESP_LOGI(TAG, "Initializing shared stack for %s", perseus_device_type_name(device_type));
}
