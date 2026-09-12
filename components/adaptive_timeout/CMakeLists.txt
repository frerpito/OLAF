idf_component_register(
    SRCS "adaptive_timeout.c"

    INCLUDE_DIRS "."

    PRIV_REQUIRES
        app_config
)