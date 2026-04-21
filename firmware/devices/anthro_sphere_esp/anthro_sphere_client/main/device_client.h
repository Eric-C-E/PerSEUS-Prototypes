#pragma once

#include "esp_err.h"

esp_err_t device_client_start(void);
esp_err_t device_client_send_event(const char *event_name);
