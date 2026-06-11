idf_component_register(
    SRCS
        "root_menu.c"
        "audio/playlist_browser.c"
        "audio/now_playing.c"
        "audio/output_select.c"
        "settings/bt_devices.c"
    INCLUDE_DIRS "."
    REQUIRES ui audio bluetooth power settings storage
)
