idf_component_register(
    SRCS "app.c" "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp storage sdcard audio input power maintenance autonomy fuel_gauge settings ui screens display_oled audio_dac gpio_expander bluetooth diag esp_pm espcoredump esp_partition
)
