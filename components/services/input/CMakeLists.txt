idf_component_register(
    SRCS "input.c" "input_logic.c"
    INCLUDE_DIRS "."
    REQUIRES bsp gpio_expander ui esp_driver_gpio esp_timer
)
