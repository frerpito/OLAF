idf_component_register(
    SRCS "wifi_component.c"

    INCLUDE_DIRS "."

    REQUIRES
        app_config
        esp_event
        esp_netif
        esp_wifi
        nvs_flash
)
