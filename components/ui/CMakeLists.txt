idf_component_register(
    SRCS "navigator.c" "menu_screen.c" "gfx/gfx.c"
    INCLUDE_DIRS "." "gfx"
    REQUIRES display_oled
)
