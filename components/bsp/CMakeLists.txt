idf_component_register(
    SRCS "spi_bus.c" "i2c_bus.c" "i2s_bus.c" "adc.c"
    INCLUDE_DIRS "."
    REQUIRES driver
)
