#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SongInfo {
    std::string filename;
    std::string title;
    size_t size_bytes;
};

class StorageManager {
public:
    bool init();
    bool is_mounted() const;
    void deinit();
    std::vector<SongInfo> scan_mp3_files(const char* dir = "/sdcard");
    size_t read_file(const std::string& path, uint8_t* buffer, size_t offset, size_t length);

private:
    bool mounted_ = false;
};
