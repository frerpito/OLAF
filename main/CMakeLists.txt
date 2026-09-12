idf_component_register(
    SRCS
        "app_main.c"
        "app_controller.c"
        "door_sensor.c"
        "temperature_ntc.c"
        "alarm.c"
        "adaptive_timeout.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_adc
        esp_timer
        driver
)