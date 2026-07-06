idf_component_register(
    SRCS "icons.c" "gfx.c" "navigator.c" "status_bar.c"
    INCLUDE_DIRS "."
    REQUIRES display_oled power audio
)
