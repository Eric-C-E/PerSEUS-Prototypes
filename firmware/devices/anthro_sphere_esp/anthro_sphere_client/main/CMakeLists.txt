file(GLOB generated_face_assets "assets/faces/generated/*.c")

idf_component_register(
    SRCS
        "app_main.c"
        "device_client.c"
        "display_init.c"
        "face_assets.c"
        "face_fsm.c"
        "face_renderer.c"
        "motion_reset.c"
        "ui_boot.c"
        "wifi_connect.c"
        ${generated_face_assets}
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_event
        esp_netif
        esp_wifi
        lwip
        nvs_flash
)
