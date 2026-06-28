idf_component_register(
    SRCS "power.c"
    INCLUDE_DIRS "."
    REQUIRES bsp fuel_gauge gpio_expander esp_driver_gpio
)
