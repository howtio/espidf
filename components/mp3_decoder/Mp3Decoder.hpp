#pragma once
#include <cstddef>
#include <cstdint>

#define MINIMP3_ONLY_MP3
#include "minimp3.h"

struct PcmFrame {
    int16_t* data;
    size_t sample_count;
    int sample_rate;
    int channels;
};

class Mp3Decoder {
public:
    bool init();
    void deinit();

    // Decode MP3 data, returns number of input bytes consumed
    size_t decode(const uint8_t* mp3_data, size_t mp3_len, PcmFrame& output);

    // Get last decoded PCM buffer (internal buffer)
    int16_t* pcm_buffer() { return pcm_buf_; }
    int pcm_sample_count() const { return pcm_samples_; }
    int sample_rate() const { return sample_rate_; }
    int channels() const { return channels_; }
    const mp3dec_frame_info_t& info() const { return info_; }

private:
    mp3dec_t mp3d_;
    mp3dec_frame_info_t info_;
    int16_t pcm_buf_[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int pcm_samples_ = 0;
    int sample_rate_ = 0;
    int channels_ = 0;
};
