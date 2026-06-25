idf_component_register(
    SRCS "display_oled.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_driver_spi esp_driver_gpio
)
