idf_component_register(
    SRCS "sdcard.c"
    INCLUDE_DIRS "."
    REQUIRES bsp fatfs sdmmc esp_driver_sdspi esp_driver_gpio
)
