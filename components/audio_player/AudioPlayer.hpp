#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstdint>
#include <string>

enum class AudioState { Stopped, Playing, Paused, Loading, Error };
struct PcmFrame;

struct AudioPlayerConfig {
    size_t mp3_read_buffer_size = 128 * 1024;
    size_t pcm_ringbuffer_size  = 256 * 1024;
    size_t i2s_dma_buffer_size  = 32 * 1024;
    int sample_rate             = 16000;
    int bits_per_sample         = 16;
    int channels                = 1;
};

class AudioPlayer {
public:
    bool init(const AudioPlayerConfig& cfg, QueueHandle_t cmd_queue, QueueHandle_t event_queue);
    void deinit();
    AudioState state() const { return state_; }
    float pcm_water_level() const { return pcm_water_level_; }
    float track_progress() const { return track_progress_; }
    uint32_t elapsed_seconds() const { return elapsed_seconds_; }
    int current_volume() const { return volume_; }
    void task_loop();

    bool play_file(const std::string& path);
    bool play_test_tone(int frequency_hz = 1000, int duration_ms = 2000);
    bool play_pcm(const std::string& path);
    void stop();
    void toggle_pause();
    void set_volume(int vol);

private:
    AudioPlayerConfig cfg_ {};
    void* tx_handle_ = nullptr;
    void* es_handle_ = nullptr;
    AudioState state_ = AudioState::Stopped;
    int volume_ = 80;
    QueueHandle_t cmd_queue_ = nullptr;
    QueueHandle_t event_queue_ = nullptr;
    uint8_t* mp3_read_buf_ = nullptr;
    uint8_t* pcm_ring_buf_ = nullptr;
    size_t pcm_ring_capacity_bytes_ = 0;
    size_t pcm_ring_head_ = 0;
    size_t pcm_ring_tail_ = 0;
    size_t pcm_ring_level_bytes_ = 0;
    int16_t* pcm_stereo_buf_ = nullptr;
    size_t pcm_stereo_buf_samples_ = 0;
    float pcm_water_level_ = 0.0f;
    float track_progress_ = 0.0f;
    uint32_t elapsed_seconds_ = 0;
    bool stop_requested_ = false;

    bool init_i2s();
    bool init_codec();
    bool init_buffers();
    void reset_pcm_ring();
    size_t pcm_ring_free_bytes() const;
    bool pcm_ring_write_frame(const PcmFrame& pcm);
    size_t pcm_ring_read_bytes(uint8_t* dst, size_t max_bytes);
    void read_and_play(const std::string& path);
};
