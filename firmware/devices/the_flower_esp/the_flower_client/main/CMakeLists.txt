idf_component_register(
    SRCS
        "app_main.c"
        "device_client.c"
        "flower_servo.c"
        "motion_reset.c"
        "wifi_connect.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_ledc
        esp_driver_i2c
        esp_event
        esp_netif
        esp_wifi
        lwip
        nvs_flash
)
