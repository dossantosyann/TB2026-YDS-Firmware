idf_component_register(
    SRCS
        "menu_screen.c"
        "root_menu.c"
        "audio/music_screen.c"
        "storage/storage_screen.c"
        "stats/stats_screen.c"
        "settings/settings_screen.c"
        "audio/playlist_browser.c"
        "audio/now_playing.c"
        "audio/output_select.c"
        "settings/bt_devices.c"
    INCLUDE_DIRS "." "audio" "storage" "stats" "settings"
    REQUIRES ui audio bluetooth power settings storage
)
