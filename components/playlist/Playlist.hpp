#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "StorageManager.hpp"

class Playlist {
public:
    void load(const std::vector<SongInfo>& songs);
    const SongInfo* current() const;
    const SongInfo* next();
    const SongInfo* prev();
    const SongInfo* jump_to(size_t index);
    size_t current_index() const;
    size_t total_count() const;
    bool is_empty() const;

private:
    std::vector<SongInfo> songs_;
    size_t index_ = 0;
};
