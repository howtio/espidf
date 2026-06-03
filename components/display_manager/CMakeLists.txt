idf_component_register(
    SRCS "DisplayManager.cpp"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../config"
    REQUIRES driver esp_lcd esp_lcd_sh8601 freertos
)
