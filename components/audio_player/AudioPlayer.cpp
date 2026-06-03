#include "AudioPlayer.hpp"
#include "MemoryPool.hpp"
#include "Mp3Decoder.hpp"
#include "board_config.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "es8311.h"
#include "esp_log.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#define I2S_NUM         I2S_NUM_0
#define I2S_MCK_IO      BOARD_I2S_MCLK_IO
#define I2S_BCK_IO      BOARD_I2S_BCLK_IO
#define I2S_WS_IO       BOARD_I2S_LRCK_IO
#define I2S_DO_IO       BOARD_I2S_DOUT_IO
#define I2S_DI_IO       BOARD_I2S_DIN_IO
#define AMP_EN_IO       BOARD_I2S_AMP_EN_IO

static const char* TAG = "AUDIO";
static constexpr int kI2SMclkMultiple = 256;
static constexpr size_t kStereoSampleBytes = sizeof(int16_t) * 2;

bool AudioPlayer::init_i2s()
{
    ESP_LOGI(TAG, "Init I2S...");
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    i2s_chan_handle_t rx_handle = nullptr;
    esp_err_t ret = i2s_new_channel(&chan_cfg, (i2s_chan_handle_t*)&tx_handle_, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return false;
    }

    i2s_std_clk_config_t clk_cfg = {};
    clk_cfg.sample_rate_hz = static_cast<uint32_t>(cfg_.sample_rate);
    clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    clk_cfg.ext_clk_freq_hz = 0;
    i2s_std_config_t std_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCK_IO, .bclk = I2S_BCK_IO, .ws = I2S_WS_IO,
            .dout = I2S_DO_IO, .din = I2S_DI_IO,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ret = i2s_channel_init_std_mode((i2s_chan_handle_t)tx_handle_, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = i2s_channel_enable((i2s_chan_handle_t)tx_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "I2S init OK: %dHz %dbit stereo, MCLK=%dHz",
             cfg_.sample_rate, cfg_.bits_per_sample, cfg_.sample_rate * kI2SMclkMultiple);
    return true;
}

bool AudioPlayer::init_codec()
{
    ESP_LOGI(TAG, "Init ES8311 codec...");

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << AMP_EN_IO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(AMP_EN_IO, 1);
    ESP_LOGI(TAG, "Amplifier enabled (GPIO%d)", AMP_EN_IO);

    es_handle_ = es8311_create(0, ES8311_ADDRESS_0);
    if (!es_handle_) { ESP_LOGE(TAG, "ES8311 create failed"); return false; }

    es8311_clock_config_t clk_cfg = {
        .mclk_inverted = false, .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = cfg_.sample_rate * kI2SMclkMultiple,
        .sample_frequency = cfg_.sample_rate,
    };
    esp_err_t ret = es8311_init((es8311_handle_t)es_handle_, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 init failed: %s", esp_err_to_name(ret));
        es8311_delete((es8311_handle_t)es_handle_);
        es_handle_ = nullptr;
        return false;
    }

    ret = es8311_sample_frequency_config((es8311_handle_t)es_handle_,
                                         cfg_.sample_rate * kI2SMclkMultiple,
                                         cfg_.sample_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 sample frequency setup failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = es8311_voice_volume_set((es8311_handle_t)es_handle_, volume_, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 volume setup failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = es8311_voice_mute((es8311_handle_t)es_handle_, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 unmute failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = es8311_microphone_config((es8311_handle_t)es_handle_, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 microphone setup failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "ES8311 codec init OK, volume=%d", volume_);
    return true;
}

bool AudioPlayer::init_buffers()
{
    MemoryPool& mem = MemoryPool::instance();

    if (!mp3_read_buf_) {
        mp3_read_buf_ = static_cast<uint8_t*>(mem.alloc_psram(cfg_.mp3_read_buffer_size));
        if (!mp3_read_buf_) {
            ESP_LOGE(TAG, "PSRAM MP3 read buffer allocation failed: %u bytes",
                     (unsigned)cfg_.mp3_read_buffer_size);
            return false;
        }
    }

    if (!pcm_stereo_buf_) {
        pcm_stereo_buf_samples_ = MINIMP3_MAX_SAMPLES_PER_FRAME * 2;
        pcm_stereo_buf_ = static_cast<int16_t*>(
            mem.alloc_sram(pcm_stereo_buf_samples_ * sizeof(int16_t)));
        if (!pcm_stereo_buf_) {
            ESP_LOGE(TAG, "SRAM PCM output buffer allocation failed: %u bytes",
                     (unsigned)(pcm_stereo_buf_samples_ * sizeof(int16_t)));
            return false;
        }
    }

    if (!pcm_ring_buf_) {
        pcm_ring_capacity_bytes_ = cfg_.pcm_ringbuffer_size - (cfg_.pcm_ringbuffer_size % kStereoSampleBytes);
        pcm_ring_buf_ = static_cast<uint8_t*>(mem.alloc_psram(pcm_ring_capacity_bytes_));
        if (!pcm_ring_buf_) {
            ESP_LOGE(TAG, "PSRAM PCM ring buffer allocation failed: %u bytes",
                     (unsigned)pcm_ring_capacity_bytes_);
            return false;
        }
        reset_pcm_ring();
    }

    ESP_LOGI(TAG, "Audio buffers ready: MP3ReadBuf=%uKB(PSRAM) PCMRingBuf=%uKB(PSRAM) PCMOutBuf=%u bytes(SRAM)",
             (unsigned)(cfg_.mp3_read_buffer_size / 1024),
             (unsigned)(pcm_ring_capacity_bytes_ / 1024),
             (unsigned)(pcm_stereo_buf_samples_ * sizeof(int16_t)));
    return true;
}

void AudioPlayer::reset_pcm_ring()
{
    pcm_ring_head_ = 0;
    pcm_ring_tail_ = 0;
    pcm_ring_level_bytes_ = 0;
    pcm_water_level_ = 0.0f;
}

size_t AudioPlayer::pcm_ring_free_bytes() const
{
    return pcm_ring_capacity_bytes_ - pcm_ring_level_bytes_;
}

bool AudioPlayer::pcm_ring_write_frame(const PcmFrame& pcm)
{
    if (!pcm_ring_buf_ || pcm.sample_count == 0) {
        return true;
    }

    const size_t frame_bytes = pcm.sample_count * kStereoSampleBytes;
    if (frame_bytes > pcm_ring_free_bytes()) {
        return false;
    }

    for (size_t i = 0; i < pcm.sample_count; ++i) {
        int16_t left = 0;
        int16_t right = 0;
        if (pcm.channels >= 2) {
            left = pcm.data[i * 2] / 2;
            right = pcm.data[i * 2 + 1] / 2;
        } else {
            left = pcm.data[i] / 2;
            right = left;
        }

        uint8_t sample_bytes[kStereoSampleBytes];
        memcpy(sample_bytes, &left, sizeof(int16_t));
        memcpy(sample_bytes + sizeof(int16_t), &right, sizeof(int16_t));

        for (size_t b = 0; b < kStereoSampleBytes; ++b) {
            pcm_ring_buf_[pcm_ring_head_] = sample_bytes[b];
            pcm_ring_head_ = (pcm_ring_head_ + 1) % pcm_ring_capacity_bytes_;
        }
    }

    pcm_ring_level_bytes_ += frame_bytes;
    pcm_water_level_ = pcm_ring_capacity_bytes_ > 0
        ? static_cast<float>(pcm_ring_level_bytes_) / static_cast<float>(pcm_ring_capacity_bytes_)
        : 0.0f;
    return true;
}

size_t AudioPlayer::pcm_ring_read_bytes(uint8_t* dst, size_t max_bytes)
{
    if (!pcm_ring_buf_ || !dst || max_bytes == 0 || pcm_ring_level_bytes_ == 0) {
        return 0;
    }

    size_t bytes_to_read = std::min(max_bytes, pcm_ring_level_bytes_);
    bytes_to_read -= (bytes_to_read % kStereoSampleBytes);
    for (size_t i = 0; i < bytes_to_read; ++i) {
        dst[i] = pcm_ring_buf_[pcm_ring_tail_];
        pcm_ring_tail_ = (pcm_ring_tail_ + 1) % pcm_ring_capacity_bytes_;
    }

    pcm_ring_level_bytes_ -= bytes_to_read;
    pcm_water_level_ = pcm_ring_capacity_bytes_ > 0
        ? static_cast<float>(pcm_ring_level_bytes_) / static_cast<float>(pcm_ring_capacity_bytes_)
        : 0.0f;
    return bytes_to_read;
}

bool AudioPlayer::init(const AudioPlayerConfig& cfg, QueueHandle_t cmd_queue, QueueHandle_t event_queue)
{
    cfg_ = cfg;
    cmd_queue_ = cmd_queue;
    event_queue_ = event_queue;
    if (!init_buffers()) return false;
    if (!init_i2s()) return false;
    if (!init_codec()) return false;
    return true;
}

void AudioPlayer::deinit()
{
    if (tx_handle_) {
        i2s_channel_disable((i2s_chan_handle_t)tx_handle_);
        i2s_del_channel((i2s_chan_handle_t)tx_handle_);
    }
    if (es_handle_) es8311_delete((es8311_handle_t)es_handle_);
    if (mp3_read_buf_) {
        free(mp3_read_buf_);
        mp3_read_buf_ = nullptr;
    }
    if (pcm_stereo_buf_) {
        free(pcm_stereo_buf_);
        pcm_stereo_buf_ = nullptr;
        pcm_stereo_buf_samples_ = 0;
    }
    if (pcm_ring_buf_) {
        free(pcm_ring_buf_);
        pcm_ring_buf_ = nullptr;
        pcm_ring_capacity_bytes_ = 0;
        reset_pcm_ring();
    }
}

bool AudioPlayer::play_file(const std::string& path)
{
    if (path.empty()) {
        return play_test_tone();
    }

    // MP3 playback
    ESP_LOGI(TAG, "MP3: %s", path.c_str());
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { ESP_LOGE(TAG, "Cannot open"); return false; }

    if (!mp3_read_buf_ || !pcm_stereo_buf_) {
        ESP_LOGE(TAG, "Playback buffers not initialized");
        fclose(f);
        return false;
    }

    Mp3Decoder dec;
    dec.init();
    reset_pcm_ring();

    state_ = AudioState::Playing;
    ESP_LOGI(TAG, "Starting streaming playback...");
    size_t mp3_off = 0;
    bool eof = false;
    int frames = 0;
    int total_samples = 0;
    const size_t mp3_buf_capacity = cfg_.mp3_read_buffer_size;
    const size_t pcm_out_chunk_bytes = pcm_stereo_buf_samples_ * sizeof(int16_t);

    while (true) {
        if (!eof && mp3_off < mp3_buf_capacity) {
            size_t rd = fread(mp3_read_buf_ + mp3_off, 1, mp3_buf_capacity - mp3_off, f);
            if (rd == 0) {
                eof = true;
            } else {
                mp3_off += rd;
            }
        }

        if (mp3_off == 0 && eof) {
            break;
        }

        PcmFrame pcm = {};
        size_t consumed = dec.decode(mp3_read_buf_, mp3_off, pcm);
        if (consumed == 0) {
            if (eof) {
                break;
            }

            if (mp3_off == mp3_buf_capacity) {
                memmove(mp3_read_buf_, mp3_read_buf_ + 1, mp3_off - 1);
                mp3_off -= 1;
            }
            continue;
        }

        memmove(mp3_read_buf_, mp3_read_buf_ + consumed, mp3_off - consumed);
        mp3_off -= consumed;
        frames++;

        if (pcm.sample_count == 0) {
            continue;
        }

        if (pcm.sample_rate != cfg_.sample_rate && frames == 1) {
            ESP_LOGW(TAG, "MP3 sample rate=%dHz, output fixed at %dHz; adjust assets or config if pitch is wrong",
                     pcm.sample_rate, cfg_.sample_rate);
        }

        while (!pcm_ring_write_frame(pcm)) {
            size_t bytes_from_ring = pcm_ring_read_bytes(reinterpret_cast<uint8_t*>(pcm_stereo_buf_), pcm_out_chunk_bytes);
            if (bytes_from_ring == 0) {
                ESP_LOGE(TAG, "PCM ringbuffer stalled: frame=%d", frames);
                fclose(f);
                dec.deinit();
                i2s_channel_disable((i2s_chan_handle_t)tx_handle_);
                state_ = AudioState::Error;
                return false;
            }

            size_t written = 0;
            esp_err_t ret = i2s_channel_write((i2s_chan_handle_t)tx_handle_, pcm_stereo_buf_,
                                              bytes_from_ring, &written, pdMS_TO_TICKS(1000));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S write failed while draining ring at frame %d: %s",
                         frames, esp_err_to_name(ret));
                fclose(f);
                dec.deinit();
                i2s_channel_disable((i2s_chan_handle_t)tx_handle_);
                state_ = AudioState::Error;
                return false;
            }
        }

        while (pcm_water_level_ >= 0.5f) {
            size_t bytes_from_ring = pcm_ring_read_bytes(reinterpret_cast<uint8_t*>(pcm_stereo_buf_), pcm_out_chunk_bytes);
            if (bytes_from_ring == 0) {
                break;
            }

            size_t written = 0;
            esp_err_t ret = i2s_channel_write((i2s_chan_handle_t)tx_handle_, pcm_stereo_buf_,
                                              bytes_from_ring, &written, pdMS_TO_TICKS(1000));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S write failed at frame %d: %s", frames, esp_err_to_name(ret));
                fclose(f);
                dec.deinit();
                i2s_channel_disable((i2s_chan_handle_t)tx_handle_);
                state_ = AudioState::Error;
                return false;
            }
        }

        total_samples += (int)pcm.sample_count;
        if (frames % 100 == 0) {
            ESP_LOGI(TAG, "Streaming frame %d, samples=%d, rate=%dHz ch=%d, PCM water=%u%%",
                     frames, total_samples, pcm.sample_rate, pcm.channels,
                     (unsigned)(pcm_water_level_ * 100.0f));
        }
    }

    while (pcm_ring_level_bytes_ > 0) {
        size_t bytes_from_ring = pcm_ring_read_bytes(reinterpret_cast<uint8_t*>(pcm_stereo_buf_), pcm_out_chunk_bytes);
        if (bytes_from_ring == 0) {
            break;
        }
        size_t written = 0;
        esp_err_t ret = i2s_channel_write((i2s_chan_handle_t)tx_handle_, pcm_stereo_buf_,
                                          bytes_from_ring, &written, pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S final drain failed: %s", esp_err_to_name(ret));
            fclose(f);
            dec.deinit();
            i2s_channel_disable((i2s_chan_handle_t)tx_handle_);
            state_ = AudioState::Error;
            return false;
        }
    }

    fclose(f);
    dec.deinit();
    ESP_LOGI(TAG, "Playback done: %d frames, %.1fs audio", frames, cfg_.sample_rate > 0 ? (float)total_samples / cfg_.sample_rate : 0.0f);
    state_ = AudioState::Stopped;
    return true;
}

bool AudioPlayer::play_test_tone(int frequency_hz, int duration_ms)
{
    ESP_LOGI(TAG, "[INF] Test tone start: %dHz %dms @ %dHz output", frequency_hz, duration_ms, cfg_.sample_rate);
    state_ = AudioState::Playing;

    constexpr int kFramesPerChunk = 512;
    int16_t buf[kFramesPerChunk * 2];
    const int total_frames = (cfg_.sample_rate * duration_ms) / 1000;
    int generated_frames = 0;
    int chunk_index = 0;

    while (generated_frames < total_frames) {
        const int frames_this_chunk = std::min(kFramesPerChunk, total_frames - generated_frames);
        for (int j = 0; j < frames_this_chunk; ++j) {
            float t = static_cast<float>(generated_frames + j) / cfg_.sample_rate;
            int16_t s = static_cast<int16_t>(sinf(2.0f * 3.14159f * frequency_hz * t) * 8000.0f * 0.3f);
            buf[j * 2] = s;
            buf[j * 2 + 1] = s;
        }

        size_t bytes_to_write = frames_this_chunk * 2 * sizeof(int16_t);
        size_t written = 0;
        esp_err_t ret = i2s_channel_write((i2s_chan_handle_t)tx_handle_, buf, bytes_to_write, &written, pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ERR] Test tone write failed at chunk %d: %s", chunk_index, esp_err_to_name(ret));
            state_ = AudioState::Error;
            return false;
        }

        generated_frames += frames_this_chunk;
        chunk_index++;
    }

    state_ = AudioState::Stopped;
    ESP_LOGI(TAG, "[INF] Test tone done: %d frames, %d chunks", generated_frames, chunk_index);
    return true;
}

bool AudioPlayer::play_pcm(const std::string& path)
{
    return play_file("");  // Just play test tone for now
}

void AudioPlayer::stop()
{
    state_ = AudioState::Stopped;
    i2s_channel_disable((i2s_chan_handle_t)tx_handle_);
}

void AudioPlayer::set_volume(int vol)
{
    volume_ = vol;
    if (es_handle_) es8311_voice_volume_set((es8311_handle_t)es_handle_, volume_, nullptr);
}

void AudioPlayer::task_loop()
{
    while (1) { vTaskDelay(pdMS_TO_TICKS(100)); }
}
