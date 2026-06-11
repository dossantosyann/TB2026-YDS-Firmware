idf_component_register(
    SRCS "spi_bus.c" "i2c_bus.c" "i2s_bus.c" "adc.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_driver_spi esp_driver_i2s esp_adc
)
