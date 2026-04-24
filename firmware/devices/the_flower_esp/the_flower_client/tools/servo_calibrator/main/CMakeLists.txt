idf_component_register(
    SRCS "servo_calibrator_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_ledc
        esp_driver_uart
)
