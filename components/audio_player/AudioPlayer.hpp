#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string>

enum class AudioState { Stopped, Playing, Paused, Loading, Error };

struct AudioPlayerConfig {
    size_t mp3_read_buffer_size = 64 * 1024;
    size_t pcm_ringbuffer_size  = 128 * 1024;
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
    float pcm_water_level() const { return 0.5f; }
    int current_volume() const { return volume_; }
    void task_loop();

    bool play_file(const std::string& path);
    bool play_pcm(const std::string& path);
    void stop();
    void set_volume(int vol);

private:
    void* tx_handle_ = nullptr;
    void* es_handle_ = nullptr;
    AudioState state_ = AudioState::Stopped;
    int volume_ = 80;
    QueueHandle_t cmd_queue_ = nullptr;
    QueueHandle_t event_queue_ = nullptr;

    bool init_i2s();
    bool init_codec();
    void read_and_play(const std::string& path);
};
