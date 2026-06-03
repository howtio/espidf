/**
 * ESP32 MP3 Player — 主入口
 */
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "SystemMonitor.hpp"
#include "DisplayManager.hpp"
#include "TouchManager.hpp"
#include "StorageManager.hpp"
#include "Playlist.hpp"
#include "AudioPlayer.hpp"

static const char* TAG = "MAIN";

#define I2C_HOST I2C_NUM_0
#define I2C_SCL  GPIO_NUM_14
#define I2C_SDA  GPIO_NUM_15

static void scan_i2c_bus()
{
    ESP_LOGI(TAG, "I2C scan starting...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_HOST, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Found device at 0x%02X", addr);
        }
    }
    ESP_LOGI(TAG, "I2C scan complete");
}

// Audio task - runs on its own stack to avoid main task overflow
struct AudioTaskCtx {
    AudioPlayer* audio;
    Playlist* playlist;
};

static void audio_task(void* arg)
{
    AudioTaskCtx* ctx = (AudioTaskCtx*)arg;
    ESP_LOGI("AUDIO", "Audio task started");

    // Skip all tests, go straight to MP3
    ESP_LOGI("AUDIO", "Playing MP3...");

    if (ctx->playlist->total_count() > 0) {
        auto* song = ctx->playlist->current();
        if (song) {
            ESP_LOGI("AUDIO", "Playing: %s", song->filename.c_str());
            ctx->audio->play_file(song->filename);
        }
    }
    ESP_LOGI("AUDIO", "Audio task done");
    ESP_LOGI("AUDIO", "Audio task done");
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    // --- Boot Banner ---
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32 MP3 Player v0.1.0");
    ESP_LOGI(TAG, "  HW: ESP32-S3-Touch-AMOLED-1.8");
    ESP_LOGI(TAG, "  CPU: 240MHz | PSRAM: 8MB | Flash: 16MB");
    ESP_LOGI(TAG, "  LCD: 368x448 AMOLED QSPI");
    ESP_LOGI(TAG, "========================================");

    // --- I2C Init ---
    ESP_LOGI(TAG, "Init I2C bus...");
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed = 400000 },
        .clk_flags = 0,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_HOST, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_HOST, i2c_cfg.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C bus OK");

    // --- I2C Scan ---
    scan_i2c_bus();

    // --- Display ---
    DisplayManager display;
    if (display.init()) {
        ESP_LOGI(TAG, "Display: SH8601 368x448 OK");
    } else {
        ESP_LOGE(TAG, "Display init FAILED");
    }

    // --- Touch ---
    QueueHandle_t event_queue = xQueueCreate(16, sizeof(uint32_t));
    TouchManager touch;
    if (touch.init(event_queue)) {
        ESP_LOGI(TAG, "Touch: FT3168 OK");
    } else {
        ESP_LOGE(TAG, "Touch init FAILED");
    }

    // --- SD Card ---
    StorageManager sd;
    Playlist playlist;
    if (sd.init()) {
        ESP_LOGI(TAG, "SD Card: mounted OK");
        auto songs = sd.scan_mp3_files("/sdcard/music");
        playlist.load(songs);
        ESP_LOGI(TAG, "Playlist: %zu songs loaded", playlist.total_count());
    } else {
        ESP_LOGE(TAG, "SD Card: mount FAILED");
    }

    // --- Audio init in main task ---
    AudioPlayer* audio = (AudioPlayer*)malloc(sizeof(AudioPlayer));
    if (audio) {
        AudioPlayerConfig cfg;
        bool ok = audio->init(cfg, nullptr, nullptr);
        ESP_LOGI(TAG, "Audio init: %s", ok ? "OK" : "FAILED");

        // Start audio playback in a separate task with large stack
        AudioTaskCtx* ctx = (AudioTaskCtx*)malloc(sizeof(AudioTaskCtx));
        ctx->audio = audio;
        ctx->playlist = &playlist;
        xTaskCreatePinnedToCore(audio_task, "audio", 16384, ctx, 10, NULL, 0);  // Core 0, highest priority
    }

    // --- System Monitor ---
    SystemMonitor& monitor = SystemMonitor::instance();
    monitor.init(nullptr);

    ESP_LOGI(TAG, "Init complete. System running...");

    // Main loop
    TouchPoint tp;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Heartbeat: alive");
        if (touch.read_touch(tp)) {
            ESP_LOGI(TAG, "Touch: x=%u y=%u pressed=%d", tp.x, tp.y, tp.pressed);
        }
    }
}
