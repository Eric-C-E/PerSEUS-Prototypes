idf_component_register(
    SRCS
        "app_main.c"
        "device_client.c"
        "lighting_controller.c"
        "motion_reset.c"
        "motor_controller.c"
        "wifi_connect.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_event
        esp_driver_i2c
        esp_driver_ledc
        esp_netif
        esp_wifi
        lwip
        nvs_flash
)