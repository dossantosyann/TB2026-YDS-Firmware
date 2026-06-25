idf_component_register(
    SRCS "app.c" "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp storage audio input power settings ui screens display_oled audio_dac gpio_expander
)
