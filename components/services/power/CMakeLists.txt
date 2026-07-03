idf_component_register(
    SRCS "power.c"
    INCLUDE_DIRS "."
    REQUIRES bsp fuel_gauge gpio_expander settings esp_driver_gpio
)
