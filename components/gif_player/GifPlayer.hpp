#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

enum class GifLevel { Full = 0, Reduced, Minimal };

class GifPlayer {
public:
    bool init();
    void deinit();
    bool load(const uint8_t* gif_data, size_t gif_size);
    bool load_from_directory(const char* frame_dir, size_t frame_count = 71);
    int decode_next_frame(uint16_t* frame_buffer, size_t buffer_size,
                          uint16_t& out_width, uint16_t& out_height);
    void set_level(GifLevel level);
    void play();
    void pause();
    bool is_playing() const;

private:
    std::string frame_dir_;
    uint16_t* frame_store_ = nullptr;
    std::array<size_t, 5> frame_slots_ {};
    size_t frame_count_ = 0;
    size_t current_frame_ = 0;
    GifLevel level_ = GifLevel::Full;
    bool playing_ = false;
};
