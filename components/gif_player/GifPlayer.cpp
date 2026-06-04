#include "GifPlayer.hpp"
#include "MemoryPool.hpp"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

static const char* TAG = "GIF";
static constexpr uint16_t kFrameWidth = 160;
static constexpr uint16_t kFrameHeight = 160;
static constexpr size_t kFramePixels = kFrameWidth * kFrameHeight;
static constexpr size_t kFrameBytes = kFramePixels * sizeof(uint16_t);

namespace {
uint16_t* s_frame_cache = nullptr;

constexpr uint16_t swap_rgb565_bytes(uint16_t value)
{
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}
}

bool GifPlayer::init()
{
    if (!s_frame_cache) {
        s_frame_cache = static_cast<uint16_t*>(MemoryPool::instance().alloc_psram(kFrameBytes));
        if (!s_frame_cache) {
            ESP_LOGE(TAG, "[ERR] GIF frame cache alloc failed: %u bytes", static_cast<unsigned>(kFrameBytes));
            return false;
        }
    }
    ESP_LOGI(TAG, "[INF] GifPlayer ready: frame cache=%u bytes", static_cast<unsigned>(kFrameBytes));
    return true;
}

void GifPlayer::deinit() {}

bool GifPlayer::load(const uint8_t* gif_data, size_t gif_size)
{
    (void)gif_data;
    (void)gif_size;
    return false;
}

bool GifPlayer::load_from_directory(const char* frame_dir, size_t frame_count)
{
    if (!frame_dir || frame_count == 0) {
        return false;
    }
    frame_dir_ = frame_dir;
    frame_count_ = frame_count;
    current_frame_ = 0;
    playing_ = true;
    ESP_LOGI(TAG, "[INF] Load preprocessed GIF frames: dir=%s frames=%u", frame_dir_.c_str(), static_cast<unsigned>(frame_count_));
    return true;
}

int GifPlayer::decode_next_frame(uint16_t* frame_buffer, size_t buffer_size,
                                  uint16_t& out_width, uint16_t& out_height)
{
    if (!playing_ || frame_dir_.empty() || frame_count_ == 0 || !frame_buffer || buffer_size < kFramePixels) {
        return 0;
    }
    if (!s_frame_cache) {
        return 0;
    }

    char path[256];
    std::snprintf(path, sizeof(path), "%s/frame_%03u.raw",
                  frame_dir_.c_str(),
                  static_cast<unsigned>(current_frame_));
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "[ERR] Open frame failed: %s", path);
        return 0;
    }

    const size_t n = std::fread(s_frame_cache, 1, kFrameBytes, f);
    std::fclose(f);
    if (n != kFrameBytes) {
        ESP_LOGE(TAG, "[ERR] Read frame short: %s bytes=%u", path, static_cast<unsigned>(n));
        return 0;
    }

    // Preprocessed raw frames are stored as big-endian RGB565 bytes on disk.
    // Normalize once here so the display pipeline reads native RGB565 values.
    for (size_t i = 0; i < kFramePixels; ++i) {
        s_frame_cache[i] = swap_rgb565_bytes(s_frame_cache[i]);
    }

    std::memcpy(frame_buffer, s_frame_cache, kFrameBytes);
    out_width = kFrameWidth;
    out_height = kFrameHeight;

    size_t step = 1;
    if (level_ == GifLevel::Reduced) {
        step = 2;
    } else if (level_ == GifLevel::Minimal) {
        step = 4;
    }
    current_frame_ = (current_frame_ + step) % frame_count_;
    return 1;
}

void GifPlayer::set_level(GifLevel level) { level_ = level; }
void GifPlayer::play() { playing_ = true; }
void GifPlayer::pause() { playing_ = false; }
bool GifPlayer::is_playing() const { return playing_; }
