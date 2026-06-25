idf_component_register(
    SRCS "audio_dac.c"
    INCLUDE_DIRS "."
    REQUIRES bsp gpio_expander
)
