#include "GifPlayer.hpp"

bool GifPlayer::init() { return true; }
void GifPlayer::deinit() {}
bool GifPlayer::load(const uint8_t* gif_data, size_t gif_size) { return true; }
int GifPlayer::decode_next_frame(uint16_t* frame_buffer, size_t buffer_size,
                                  uint16_t& out_width, uint16_t& out_height) { return 0; }
void GifPlayer::set_level(GifLevel level) {}
void GifPlayer::play() {}
void GifPlayer::pause() {}
bool GifPlayer::is_playing() const { return false; }
