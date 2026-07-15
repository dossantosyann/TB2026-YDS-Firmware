idf_component_register(
    SRCS "autonomy.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer power storage sdcard nvs_flash audio
)
