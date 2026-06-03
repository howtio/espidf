#include "AudioPlayer.hpp"
#include "Mp3Decoder.hpp"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "es8311.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <cmath>
#include <cstring>
#include <cstdio>

#define I2S_NUM         I2S_NUM_0
#define I2S_MCK_IO      GPIO_NUM_16
#define I2S_BCK_IO      GPIO_NUM_9
#define I2S_WS_IO       GPIO_NUM_45
#define I2S_DO_IO       GPIO_NUM_8
#define I2S_DI_IO       GPIO_NUM_10
#define AMP_EN_IO       GPIO_NUM_46
#define SAMPLE_RATE     16000

static const char* TAG = "AUDIO";

bool AudioPlayer::init_i2s()
{
    ESP_LOGI(TAG, "Init I2S...");
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    i2s_chan_handle_t rx_handle = nullptr;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, (i2s_chan_handle_t*)&tx_handle_, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCK_IO, .bclk = I2S_BCK_IO, .ws = I2S_WS_IO,
            .dout = I2S_DO_IO, .din = I2S_DI_IO,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode((i2s_chan_handle_t)tx_handle_, &std_cfg));
    // Don't enable yet - will enable just before playback
    ESP_LOGI(TAG, "I2S init OK: %dHz 16bit stereo", SAMPLE_RATE);
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
        .mclk_frequency = SAMPLE_RATE * 384,
        .sample_frequency = SAMPLE_RATE,
    };
    ESP_ERROR_CHECK(es8311_init((es8311_handle_t)es_handle_, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
    ESP_ERROR_CHECK(es8311_sample_frequency_config((es8311_handle_t)es_handle_, SAMPLE_RATE * 384, SAMPLE_RATE));
    ESP_ERROR_CHECK(es8311_voice_volume_set((es8311_handle_t)es_handle_, volume_, nullptr));
    ESP_ERROR_CHECK(es8311_microphone_config((es8311_handle_t)es_handle_, false));
    ESP_LOGI(TAG, "ES8311 codec init OK, volume=%d", volume_);
    return true;
}

bool AudioPlayer::init(const AudioPlayerConfig& cfg, QueueHandle_t cmd_queue, QueueHandle_t event_queue)
{
    cmd_queue_ = cmd_queue;
    event_queue_ = event_queue;
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
}

bool AudioPlayer::play_file(const std::string& path)
{
    if (path.empty()) {
        // Test tone
        ESP_LOGI(TAG, "Test tone: 1kHz 3s");
        state_ = AudioState::Playing;
        i2s_channel_enable((i2s_chan_handle_t)tx_handle_);

        int16_t buf[1024];
        for (int i = 0; i < SAMPLE_RATE * 3; i += 512) {
            for (int j = 0; j < 512; j++) {
                float t = (float)(i + j) / SAMPLE_RATE;
                int16_t s = (int16_t)(sinf(2.0f * 3.14159f * 1000.0f * t) * 8000.0f * 0.3f);
                buf[j*2] = s;
                buf[j*2+1] = s;
            }
            size_t w = 0;
            i2s_channel_write((i2s_chan_handle_t)tx_handle_, buf, sizeof(buf), &w, pdMS_TO_TICKS(1000));
        }
        state_ = AudioState::Stopped;
        return true;
    }

    // MP3 playback
    ESP_LOGI(TAG, "MP3: %s", path.c_str());
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { ESP_LOGE(TAG, "Cannot open"); return false; }

    Mp3Decoder* dec = (Mp3Decoder*)malloc(sizeof(Mp3Decoder));
    if (!dec) { fclose(f); return false; }
    dec->init();

    // Allocate output PCM buffer (PSRAM)
    const int MAX_OUT = SAMPLE_RATE * 240;  // 4 min
    int16_t* out_pcm = (int16_t*)malloc(MAX_OUT * sizeof(int16_t));
    int out_samples = 0;

    // Decode entire MP3 to memory
    uint8_t mp3_buf[4096];
    size_t mp3_off = 0;
    int frames = 0;

    while (out_samples < MAX_OUT) {
        if (mp3_off < sizeof(mp3_buf)) {
            size_t rd = fread(mp3_buf + mp3_off, 1, sizeof(mp3_buf) - mp3_off, f);
            if (rd == 0) break;
            mp3_off += rd;
        }
        PcmFrame pcm;
        size_t consumed = dec->decode(mp3_buf, mp3_off, pcm);
        if (consumed == 0) {
            if (mp3_off >= sizeof(mp3_buf)) break;
            continue;
        }
        memmove(mp3_buf, mp3_buf + consumed, mp3_off - consumed);
        mp3_off -= consumed;
        frames++;
        if (pcm.sample_count > 0) {
            if (out_samples + pcm.sample_count > MAX_OUT) break;
            memcpy(out_pcm + out_samples, pcm.data, pcm.sample_count * sizeof(int16_t));
            out_samples += pcm.sample_count;
        }
        if (frames % 100 == 0) {
            ESP_LOGI(TAG, "Decoded %d frames, %d samples", frames, out_samples);
            esp_task_wdt_reset();
        }
    }
    fclose(f);
    free(dec);
    ESP_LOGI(TAG, "Decode done: %d samples (%.1fs)", out_samples, (float)out_samples / SAMPLE_RATE);

    // Playback from memory
    state_ = AudioState::Playing;
    ESP_LOGI(TAG, "Starting I2S playback...");
    i2s_channel_enable((i2s_chan_handle_t)tx_handle_);

    int16_t buf[1024];
    for (int i = 0; i < out_samples; i += 512) {
        int n = (i + 512 > out_samples) ? (out_samples - i) : 512;
        for (int j = 0; j < n; j++) {
            int16_t s = out_pcm[i + j] / 2;
            buf[j*2] = s;
            buf[j*2+1] = s;
        }
        size_t w = 0;
        i2s_channel_write((i2s_chan_handle_t)tx_handle_, buf, n * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(1000));
        if (i % 16000 == 0) esp_task_wdt_reset();
    }

    free(out_pcm);
    ESP_LOGI(TAG, "Playback done: %d frames", frames);
    state_ = AudioState::Stopped;
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
