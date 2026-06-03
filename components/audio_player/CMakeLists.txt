idf_component_register(
    SRCS "AudioPlayer.cpp"
    INCLUDE_DIRS "." "../../config"
    REQUIRES driver es8311 mp3_decoder storage_manager memory_pool
)
