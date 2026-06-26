idf_component_register(
    SRCS "bluetooth.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash
)
