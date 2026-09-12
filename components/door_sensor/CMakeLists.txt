idf_component_register(
    SRCS "door_sensor.c"

    INCLUDE_DIRS "."

    REQUIRES
        esp_driver_gpio
        app_config

    PRIV_REQUIRES
        esp_timer
)