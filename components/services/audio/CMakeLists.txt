idf_component_register(
    SRCS "pipeline.c" "player.c" "playlist.c" "sink_i2s_dac.c" "sink_bluetooth.c" "volume.c"
         "decoder.c" "decoder_mp3.c" "decoder_wav.c" "mp3_frame.c" "track_meta.c"
    INCLUDE_DIRS "."
    REQUIRES bsp audio_dac gpio_expander storage bluetooth settings diag chmorgan__esp-libhelix-mp3
)
