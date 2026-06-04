idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        audio_player
        mp3_decoder
        storage_manager
        display_manager
        gif_player
        touch_manager
        button_manager
        playlist
        memory_pool
        system_monitor
        esp_timer
        spiffs
)

spiffs_create_partition_image(assets ../assets/flash FLASH_IN_PROJECT)
