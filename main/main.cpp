/**
 * ESP32 MP3 Player — 主入口
 */
#include <string>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "SystemMonitor.hpp"
#include "DisplayManager.hpp"
#include "GifPlayer.hpp"
#include "TouchManager.hpp"
#include "StorageManager.hpp"
#include "Playlist.hpp"
#include "AudioPlayer.hpp"
#include "MemoryPool.hpp"

static const char* TAG = "MAIN";

enum class TransportCommand {
    None = 0,
    Next,
    Prev,
};

enum class StartupAudioMode {
    TestTone,
    Playlist,
    TestThenPlaylist,
};

static constexpr StartupAudioMode kStartupAudioMode = StartupAudioMode::TestThenPlaylist;

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
    DisplayManager* display;
};

static AudioPlayer s_audio;
static Playlist s_playlist;
static volatile TransportCommand s_transport_command = TransportCommand::None;

static bool mount_assets_partition()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/assets",
        .partition_label = "assets",
        .max_files = 8,
        .format_if_mount_failed = false,
    };
    const esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("ASSET", "[ERR] SPIFFS mount failed: %s", esp_err_to_name(ret));
        return false;
    }

    size_t total = 0;
    size_t used = 0;
    if (esp_spiffs_info("assets", &total, &used) == ESP_OK) {
        ESP_LOGI("ASSET", "[INF] Assets SPIFFS mounted: used=%uKB total=%uKB",
                 static_cast<unsigned>(used / 1024),
                 static_cast<unsigned>(total / 1024));
    }
    return true;
}

static const char* control_name(UiControl control)
{
    switch (control) {
        case UiControl::Prev: return "Prev";
        case UiControl::PlayPause: return "PlayPause";
        case UiControl::Next: return "Next";
        case UiControl::None:
        default: return "None";
    }
}

static UiControl resolve_touch_control(const DisplayManager& display, const TouchPoint& pt, uint16_t& out_x, uint16_t& out_y)
{
    struct Candidate { uint16_t x; uint16_t y; };
    const uint16_t w = display.screen_width();
    const uint16_t h = display.screen_height();
    const Candidate candidates[] = {
        {pt.x, pt.y},
        {pt.y, static_cast<uint16_t>(pt.x < h ? (h - 1 - pt.x) : 0)},
        {static_cast<uint16_t>(pt.x < w ? (w - 1 - pt.x) : 0), static_cast<uint16_t>(pt.y < h ? (h - 1 - pt.y) : 0)},
        {static_cast<uint16_t>(pt.y < w ? (w - 1 - pt.y) : 0), pt.x},
    };

    for (const Candidate& c : candidates) {
        UiControl control = display.hit_test_control(c.x, c.y);
        if (control != UiControl::None) {
            out_x = c.x;
            out_y = c.y;
            return control;
        }
    }

    out_x = pt.x;
    out_y = pt.y;
    return UiControl::None;
}

static void play_playlist_loop(AudioTaskCtx* ctx, const char* log_prefix)
{
    if (ctx->playlist->is_empty()) {
        ESP_LOGW("AUDIO", "[WRN] %s skipped: playlist empty", log_prefix);
        return;
    }

    const size_t total = ctx->playlist->total_count();
    size_t played_count = 0;

    while (true) {
        const SongInfo* song = ctx->playlist->current();
        if (!song) {
            ESP_LOGE("AUDIO", "[ERR] %s current song is null", log_prefix);
            return;
        }

        ESP_LOGI("AUDIO", "[INF] %s track %u/%u: %s",
                 log_prefix,
                 (unsigned)(ctx->playlist->current_index() + 1),
                 (unsigned)total,
                 song->filename.c_str());
        if (ctx->display) {
            ctx->display->update_now_playing(song->filename.c_str(),
                                             ctx->playlist->current_index(),
                                             total);
            ctx->display->update_playback_meter(ctx->audio->pcm_water_level(), true);
        }

        bool ok = ctx->audio->play_file(song->filename);
        ESP_LOGI("AUDIO", "[INF] Track finished: %s (%s)",
                 song->filename.c_str(),
                 ok ? "ok" : "error");

        played_count++;
        TransportCommand pending = s_transport_command;
        s_transport_command = TransportCommand::None;
        const SongInfo* next_song = nullptr;
        if (pending == TransportCommand::Prev) {
            ESP_LOGI("AUDIO", "[INF] Transport command: previous");
            next_song = ctx->playlist->prev();
        } else {
            if (pending == TransportCommand::Next) {
                ESP_LOGI("AUDIO", "[INF] Transport command: next");
            }
            next_song = ctx->playlist->next();
        }
        if (!next_song) {
            ESP_LOGW("AUDIO", "[WRN] Playlist next() returned null, stop loop");
            return;
        }

        if ((played_count % total) == 0) {
            ESP_LOGI("AUDIO", "[INF] Playlist loop complete, wrapping to first track");
        }
    }
}

static void audio_task(void* arg)
{
    AudioTaskCtx* ctx = (AudioTaskCtx*)arg;
    ESP_LOGI("AUDIO", "Audio task started");

    if (kStartupAudioMode == StartupAudioMode::TestTone) {
        ESP_LOGI("AUDIO", "[INF] Startup mode: test tone baseline");
        if (ctx->display) {
            ctx->display->update_now_playing("TEST TONE 1KHZ", 0, 0);
            ctx->display->update_playback_meter(0.0f, true);
            ctx->display->update_transport_controls(true, UiControl::None);
        }
        ctx->audio->play_test_tone(1000, 2000);
    } else if (kStartupAudioMode == StartupAudioMode::TestThenPlaylist) {
        ESP_LOGI("AUDIO", "[INF] Startup mode: test tone -> playlist A/B");
        if (ctx->display) {
            ctx->display->update_now_playing("TEST TONE 1KHZ", 0, 0);
            ctx->display->update_playback_meter(0.0f, true);
            ctx->display->update_transport_controls(true, UiControl::None);
        }
        ctx->audio->play_test_tone(1000, 2000);
        play_playlist_loop(ctx, "A/B step 2");
    } else if (ctx->playlist->total_count() > 0) {
        ESP_LOGI("AUDIO", "[INF] Startup mode: SD playlist");
        play_playlist_loop(ctx, "Playlist");
    } else {
        ESP_LOGW("AUDIO", "[WRN] No MP3 found on SD card, fallback to test tone");
        ctx->audio->play_test_tone(1000, 2000);
    }
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

    SystemMonitor& monitor = SystemMonitor::instance();
    MemoryPool& memory_pool = MemoryPool::instance();
    monitor.init(nullptr);
    memory_pool.init();
    monitor.print_boot_banner();
    const bool assets_ok = mount_assets_partition();

    // --- Display ---
    DisplayManager display;
    if (display.init()) {
        ESP_LOGI(TAG, "Display: SH8601 368x448 OK");
    } else {
        ESP_LOGE(TAG, "Display init FAILED");
    }
    SystemStatus boot_status = monitor.collect();
    boot_status.pcm_water_level = s_audio.pcm_water_level();
    display.update_status_bar(boot_status.battery_percent,
                              boot_status.is_charging,
                              boot_status.sram_free,
                              boot_status.psram_free,
                              boot_status.pcm_water_level);

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
    GifPlayer gif;
    bool gif_ready = false;
    if (sd.init()) {
        ESP_LOGI(TAG, "SD Card: mounted OK");
        auto songs = sd.scan_mp3_files("/sdcard/music");
        s_playlist.load(songs);
        ESP_LOGI(TAG, "Playlist: %zu songs loaded", s_playlist.total_count());
    } else {
        ESP_LOGE(TAG, "SD Card: mount FAILED");
    }

    if (assets_ok && gif.init() && gif.load_from_directory("/assets/pet/frames", 71)) {
        gif_ready = true;
        ESP_LOGI("GIF", "[INF] Built-in flash GIF frames ready from /assets/pet/frames");
    } else if (gif.init() && gif.load_from_directory("/sdcard/pet/frames", 71)) {
        gif_ready = true;
        ESP_LOGW("GIF", "[WRN] Flash GIF unavailable, fallback to /sdcard/pet/frames");
    } else {
        ESP_LOGW("GIF", "[WRN] GIF frames unavailable, keep static placeholder");
    }

    // --- Audio init in main task ---
    AudioPlayerConfig cfg;
    bool ok = s_audio.init(cfg, nullptr, nullptr);
    ESP_LOGI(TAG, "Audio init: %s", ok ? "OK" : "FAILED");
    if (ok) {
        const char* startup_mode = "playlist";
        if (kStartupAudioMode == StartupAudioMode::TestTone) {
            startup_mode = "test-tone";
        } else if (kStartupAudioMode == StartupAudioMode::TestThenPlaylist) {
            startup_mode = "test-then-playlist";
        }
        ESP_LOGI(TAG, "Audio startup mode: %s", startup_mode);
        static AudioTaskCtx ctx {
            .audio = &s_audio,
            .playlist = &s_playlist,
            .display = &display,
        };
        xTaskCreatePinnedToCore(audio_task, "audio", 32768, &ctx, 10, NULL, 0);
    }

    ESP_LOGI(TAG, "Init complete. System running...");

    // Main loop
    TouchPoint tp;
    bool touch_active = false;
    UiControl highlighted = UiControl::None;
    UiControl pressed_control = UiControl::None;
    uint32_t touch_release_samples = 0;
    uint32_t tick_250ms = 0;
    uint32_t tick_500ms = 0;
    uint32_t tick_5s = 0;
    UiControl last_rendered_highlight = UiControl::None;
    bool last_rendered_playing = false;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
        tick_250ms += 20;
        tick_500ms += 20;
        tick_5s += 20;

        if (tick_250ms >= 250) {
            tick_250ms = 0;
            display.update_playback_meter(s_audio.track_progress(), s_audio.state() == AudioState::Playing);
            if (gif_ready && gif.is_playing()) {
                static uint16_t* gif_frame = nullptr;
                if (!gif_frame) {
                    gif_frame = static_cast<uint16_t*>(MemoryPool::instance().alloc_psram(160 * 160 * sizeof(uint16_t)));
                }
                if (gif_frame) {
                    uint16_t w = 0;
                    uint16_t h = 0;
                    if (gif.decode_next_frame(gif_frame, 160 * 160, w, h) > 0) {
                        display.render_gif_frame_rgb565(gif_frame, w, h);
                    }
                }
            }
        }

        if (tick_500ms >= 500) {
            tick_500ms = 0;
            const bool is_playing = s_audio.state() == AudioState::Playing;
            if (last_rendered_playing != is_playing || last_rendered_highlight != highlighted) {
                display.update_transport_controls(is_playing, highlighted);
                last_rendered_playing = is_playing;
                last_rendered_highlight = highlighted;
            }
        }

        if (touch.read_touch(tp)) {
            touch_release_samples = 0;
            uint16_t mapped_x = tp.x;
            uint16_t mapped_y = tp.y;
            UiControl control = resolve_touch_control(display, tp, mapped_x, mapped_y);
            if (!touch_active) {
                touch_active = true;
                pressed_control = control;
                highlighted = control;
                display.update_transport_controls(s_audio.state() == AudioState::Playing, highlighted);
                last_rendered_highlight = highlighted;
                last_rendered_playing = (s_audio.state() == AudioState::Playing);
                if (control != UiControl::None) {
                    ESP_LOGI("TOUCH", "[INF] Control down: raw=(%u,%u) mapped=(%u,%u) control=%s",
                             tp.x, tp.y, mapped_x, mapped_y, control_name(control));
                } else {
                    ESP_LOGI("TOUCH", "[INF] Touch down outside controls: raw=(%u,%u) mapped=(%u,%u)",
                             tp.x, tp.y, mapped_x, mapped_y);
                }
            } else if (highlighted != control) {
                highlighted = control;
                display.update_transport_controls(s_audio.state() == AudioState::Playing, highlighted);
                last_rendered_highlight = highlighted;
                last_rendered_playing = (s_audio.state() == AudioState::Playing);
            }
        } else {
            touch_release_samples++;
            if (touch_active && touch_release_samples >= 2) {
                touch_active = false;
                if (pressed_control != UiControl::None && highlighted == pressed_control) {
                    ESP_LOGI("TOUCH", "[INF] Control tap: %s", control_name(pressed_control));
                    if (pressed_control == UiControl::PlayPause) {
                        s_audio.toggle_pause();
                    } else if (pressed_control == UiControl::Next) {
                        s_transport_command = TransportCommand::Next;
                        s_audio.stop();
                    } else if (pressed_control == UiControl::Prev) {
                        s_transport_command = TransportCommand::Prev;
                        s_audio.stop();
                    }
                }
                highlighted = UiControl::None;
                pressed_control = UiControl::None;
                display.update_transport_controls(s_audio.state() == AudioState::Playing, highlighted);
                last_rendered_highlight = highlighted;
                last_rendered_playing = (s_audio.state() == AudioState::Playing);
            }
        }

        if (tick_5s >= 5000) {
            tick_5s = 0;
            ESP_LOGI(TAG, "Heartbeat: alive");
            SystemStatus st = monitor.collect();
            st.pcm_water_level = s_audio.pcm_water_level();
            monitor.print_status(st);
            display.update_status_bar(st.battery_percent,
                                      st.is_charging,
                                      st.sram_free,
                                      st.psram_free,
                                      st.pcm_water_level);
        }
    }
}
