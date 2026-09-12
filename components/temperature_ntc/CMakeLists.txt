idf_component_register(
    SRCS "temperature_ntc.c"

    INCLUDE_DIRS "."

    REQUIRES
        esp_adc
        app_config
)