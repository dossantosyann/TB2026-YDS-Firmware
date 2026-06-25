idf_component_register(
    SRCS "pipeline.c" "playlist.c" "sink_i2s_dac.c" "sink_bluetooth.c" "volume.c"
    INCLUDE_DIRS "."
    REQUIRES bsp audio_dac gpio_expander storage bluetooth
)
