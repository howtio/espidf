idf_component_register(
    SRCS "StorageManager.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver fatfs sdmmc vfs
)
