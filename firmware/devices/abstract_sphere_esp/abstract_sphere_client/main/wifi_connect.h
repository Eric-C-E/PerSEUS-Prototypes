#pragma once

#include "esp_err.h"
#include "esp_netif.h"

esp_err_t wifi_connect_start(void);
esp_err_t wifi_connect_get_ip_info(esp_netif_ip_info_t *out_info);
