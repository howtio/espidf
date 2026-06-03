#pragma once
#include <cstddef>
#include <cstdint>

enum class GifLevel { Full = 0, Reduced, Minimal };

class GifPlayer {
public:
    bool init();
    void deinit();
    bool load(const uint8_t* gif_data, size_t gif_size);
    int decode_next_frame(uint16_t* frame_buffer, size_t buffer_size,
                          uint16_t& out_width, uint16_t& out_height);
    void set_level(GifLevel level);
    void play();
    void pause();
    bool is_playing() const;
};
