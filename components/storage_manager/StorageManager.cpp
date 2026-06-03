#include "StorageManager.hpp"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

#define MOUNT_POINT "/sdcard"

bool StorageManager::init()
{
    ESP_LOGI("SD", "Initializing SD card (SDMMC 1-bit)...");

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_cfg.width = 1;
    slot_cfg.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    slot_cfg.clk = GPIO_NUM_2;
    slot_cfg.cmd = GPIO_NUM_1;
    slot_cfg.d0 = GPIO_NUM_3;
#endif

    sdmmc_card_t* card = nullptr;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &card);

    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Mount failed: %s", esp_err_to_name(ret));
        return false;
    }

    sdmmc_card_print_info(stdout, card);
    mounted_ = true;
    ESP_LOGI("SD", "SD card mounted at %s", MOUNT_POINT);
    return true;
}

bool StorageManager::is_mounted() const
{
    return mounted_;
}

void StorageManager::deinit()
{
    if (mounted_) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, nullptr);
        mounted_ = false;
    }
}

std::vector<SongInfo> StorageManager::scan_mp3_files(const char* dir)
{
    std::vector<SongInfo> songs;
    if (!mounted_) return songs;

    ESP_LOGI("SD", "Scanning %s for MP3 files...", dir);
    DIR* d = opendir(dir);
    if (!d) {
        ESP_LOGE("SD", "Failed to open directory: %s", dir);
        return songs;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name(entry->d_name);
        if (name.length() < 5) continue;

        // Check for .mp3 or .MP3 extension
        std::string ext = name.substr(name.length() - 4);
        bool is_mp3 = (ext == ".mp3" || ext == ".MP3" || ext == ".Mp3");
        if (!is_mp3) continue;

        std::string full_path = std::string(dir) + "/" + name;
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0) continue;

        SongInfo info;
        info.filename = full_path;
        info.title = name;
        info.size_bytes = st.st_size;
        songs.push_back(info);
        ESP_LOGI("SD", "  %s (%lu KB)", name.c_str(), (unsigned long)(st.st_size / 1024));
    }
    closedir(d);
    ESP_LOGI("SD", "Found %lu MP3 files", (unsigned long)songs.size());
    return songs;
}

size_t StorageManager::read_file(const std::string& path, uint8_t* buffer, size_t offset, size_t length)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return 0;
    fseek(f, offset, SEEK_SET);
    size_t read = fread(buffer, 1, length, f);
    fclose(f);
    return read;
}
