idf_component_register(
    SRCS "app.c" "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp storage audio input power maintenance fuel_gauge settings ui screens display_oled audio_dac gpio_expander bluetooth esp_pm
)
