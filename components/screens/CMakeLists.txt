idf_component_register(
    SRCS
        "menu_screen.c"
        "root_menu.c"
        "storage/storage_screen.c"
        "stats/stats_screen.c"
        "settings/settings_screen.c"
        "audio/playlist_browser.c"
        "audio/now_playing.c"
        "audio/output_select.c"
        "settings/bluetooth_settings.c"
        "settings/audio_settings.c"
        "settings/screen_settings.c"
    INCLUDE_DIRS "." "audio" "storage" "stats" "settings"
    REQUIRES ui audio audio_dac bluetooth power settings storage input bsp fuel_gauge sdcard display_oled esp_timer esp_app_format
)
