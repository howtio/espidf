#include "GifPlayer.hpp"
#include "MemoryPool.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

static const char* TAG = "GIF";
static constexpr uint16_t kFrameWidth = 160;
static constexpr uint16_t kFrameHeight = 160;
static constexpr size_t kFramePixels = kFrameWidth * kFrameHeight;
static constexpr size_t kFrameBytes = kFramePixels * sizeof(uint16_t);
static constexpr size_t kFrameWindow = 5;

namespace {
constexpr uint16_t swap_rgb565_bytes(uint16_t value)
{
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}

bool load_frame_file(const char* frame_dir, size_t frame_index, uint16_t* dst)
{
    char path[256];
    std::snprintf(path, sizeof(path), "%s/frame_%03u.raw",
                  frame_dir,
                  static_cast<unsigned>(frame_index));
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "[ERR] Open frame failed: %s", path);
        return false;
    }

    const size_t n = std::fread(dst, 1, kFrameBytes, f);
    std::fclose(f);
    if (n != kFrameBytes) {
        ESP_LOGE(TAG, "[ERR] Read frame short: %s bytes=%u", path, static_cast<unsigned>(n));
        return false;
    }

    for (size_t i = 0; i < kFramePixels; ++i) {
        dst[i] = swap_rgb565_bytes(dst[i]);
    }
    return true;
}
}

bool GifPlayer::init()
{
    ESP_LOGI(TAG, "[INF] GifPlayer ready: frame size=%u bytes", static_cast<unsigned>(kFrameBytes));
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

    const size_t window_frames = std::min(frame_count, kFrameWindow);
    const size_t total_bytes = window_frames * kFrameBytes;
    if (!frame_store_) {
        frame_store_ = static_cast<uint16_t*>(MemoryPool::instance().alloc_psram(total_bytes));
        if (!frame_store_) {
            ESP_LOGE(TAG, "[ERR] GIF frame store alloc failed: %u bytes", static_cast<unsigned>(total_bytes));
            return false;
        }
    }

    frame_dir_ = frame_dir;
    frame_count_ = frame_count;
    current_frame_ = 0;
    playing_ = true;
    frame_slots_.fill(SIZE_MAX);
    ESP_LOGI(TAG, "[INF] Prime GIF window: dir=%s total=%u window=%u bytes=%u",
             frame_dir_.c_str(),
             static_cast<unsigned>(frame_count_),
             static_cast<unsigned>(window_frames),
             static_cast<unsigned>(total_bytes));

    for (size_t slot = 0; slot < window_frames; ++slot) {
        uint16_t* dst = frame_store_ + (slot * kFramePixels);
        if (!load_frame_file(frame_dir_.c_str(), slot, dst)) {
            return false;
        }
        frame_slots_[slot] = slot;
    }
    ESP_LOGI(TAG, "[INF] GIF window primed: %u frames", static_cast<unsigned>(window_frames));
    return true;
}

int GifPlayer::decode_next_frame(uint16_t* frame_buffer, size_t buffer_size,
                                  uint16_t& out_width, uint16_t& out_height)
{
    if (!playing_ || frame_dir_.empty() || frame_count_ == 0 || !frame_buffer || buffer_size < kFramePixels) {
        return 0;
    }
    if (!frame_store_) {
        return 0;
    }

    size_t current_slot = SIZE_MAX;
    for (size_t slot = 0; slot < frame_slots_.size(); ++slot) {
        if (frame_slots_[slot] == current_frame_) {
            current_slot = slot;
            break;
        }
    }
    if (current_slot == SIZE_MAX) {
        current_slot = current_frame_ % frame_slots_.size();
        uint16_t* dst = frame_store_ + (current_slot * kFramePixels);
        if (!load_frame_file(frame_dir_.c_str(), current_frame_, dst)) {
            return 0;
        }
        frame_slots_[current_slot] = current_frame_;
    }

    const uint16_t* src = frame_store_ + (current_slot * kFramePixels);
    std::memcpy(frame_buffer, src, kFrameBytes);
    out_width = kFrameWidth;
    out_height = kFrameHeight;

    size_t step = 1;
    if (level_ == GifLevel::Reduced) {
        step = 2;
    } else if (level_ == GifLevel::Minimal) {
        step = 4;
    }
    current_frame_ = (current_frame_ + step) % frame_count_;

    const size_t tail_frame = (current_frame_ + (kFrameWindow - 1)) % frame_count_;
    bool tail_cached = false;
    for (size_t slot = 0; slot < frame_slots_.size(); ++slot) {
        if (frame_slots_[slot] == tail_frame) {
            tail_cached = true;
            break;
        }
    }
    if (!tail_cached) {
        uint16_t* dst = frame_store_ + (current_slot * kFramePixels);
        if (load_frame_file(frame_dir_.c_str(), tail_frame, dst)) {
            frame_slots_[current_slot] = tail_frame;
        }
    }
    return 1;
}

void GifPlayer::set_level(GifLevel level) { level_ = level; }
void GifPlayer::play() { playing_ = true; }
void GifPlayer::pause() { playing_ = false; }
bool GifPlayer::is_playing() const { return playing_; }
