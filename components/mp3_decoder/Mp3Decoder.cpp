#define MINIMP3_IMPLEMENTATION
#include "Mp3Decoder.hpp"
#include "esp_log.h"
#include <cstring>

bool Mp3Decoder::init()
{
    mp3dec_init(&mp3d_);
    memset(&info_, 0, sizeof(info_));
    ESP_LOGI("MP3", "Minimp3 decoder initialized");
    return true;
}

void Mp3Decoder::deinit() {}

size_t Mp3Decoder::decode(const uint8_t* mp3_data, size_t mp3_len, PcmFrame& output)
{
    output.data = nullptr;
    output.sample_count = 0;
    output.sample_rate = 0;
    output.channels = 0;

    pcm_samples_ = mp3dec_decode_frame(&mp3d_, mp3_data, (int)mp3_len, pcm_buf_, &info_);
    if (pcm_samples_ <= 0) {
        pcm_samples_ = 0;
        return info_.frame_bytes > 0 ? (size_t)info_.frame_bytes : 0;
    }

    sample_rate_ = info_.hz;
    channels_ = info_.channels;
    output.data = pcm_buf_;
    output.sample_count = pcm_samples_;
    output.sample_rate = info_.hz;
    output.channels = info_.channels;
    return info_.frame_bytes;
}
