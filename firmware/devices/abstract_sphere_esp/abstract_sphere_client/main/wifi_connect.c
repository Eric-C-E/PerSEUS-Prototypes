#include "wifi_connect.h"

#include <inttypes.h>
#include <string.h>

#include "client_config.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_connect";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_RETRY_LOG_PERIOD 10

static EventGroupHandle_t s_wifi_event_group;
static uint32_t s_retry_count;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        s_retry_count++;
        if (s_retry_count == 1 || (s_retry_count % WIFI_RETRY_LOG_PERIOD) == 0) {
            ESP_LOGW(TAG, "Wi-Fi disconnected; retry %" PRIu32 " and will keep trying", s_retry_count);
        }

        esp_err_t ret = esp_wifi_connect();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Wi-Fi reconnect request failed: %s", esp_err_to_name(ret));
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_connect_start(void)
{
    ESP_LOGI(TAG, "Initialize Wi-Fi station");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "NVS init failed");

    s_wifi_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_wifi_event_group, ESP_ERR_NO_MEM, TAG, "Wi-Fi event group allocation failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop init failed");
    esp_netif_create_default_wifi_sta();

    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "Wi-Fi init failed");

    esp_event_handler_instance_t wifi_event_instance;
    esp_event_handler_instance_t ip_event_instance;
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            &wifi_event_instance),
                        TAG,
                        "Wi-Fi event handler register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            &ip_event_instance),
                        TAG,
                        "IP event handler register failed");

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, DEVICE_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, DEVICE_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Wi-Fi config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");

    ESP_LOGI(TAG, "Connecting to SSID '%s'", DEVICE_WIFI_SSID);
    const EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                                 WIFI_CONNECTED_BIT,
                                                 pdFALSE,
                                                 pdFALSE,
                                                 portMAX_DELAY);

    ESP_RETURN_ON_FALSE((bits & WIFI_CONNECTED_BIT) != 0, ESP_FAIL, TAG, "Failed to connect to Wi-Fi");

    return ESP_OK;
}
