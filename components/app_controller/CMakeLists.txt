idf_component_register(
    SRCS "app_controller.c"

    INCLUDE_DIRS "."

    REQUIRES
        door_sensor
        temperature_ntc
        alarm
        adaptive_timeout
        app_config

    PRIV_REQUIRES
        esp_timer
)