#pragma once
#include <cstdint>
#include <cstddef>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"

enum class UiControl {
    None = 0,
    Prev,
    PlayPause,
    Next,
};

class DisplayManager {
public:
    bool init();
    void deinit();
    void flush_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data);
    void* lvgl_display_driver();
    void render_static_ui();
    void update_status_bar(uint8_t battery_percent,
                           bool is_charging,
                           size_t sram_free_bytes,
                           size_t psram_free_bytes,
                           float pcm_water_level);
    void update_now_playing(const char* title, size_t index, size_t total);
    void update_playback_meter(float ratio, bool is_playing);
    void update_transport_controls(bool is_playing, UiControl highlighted);
    UiControl hit_test_control(uint16_t x, uint16_t y) const;
    void animate_gif_placeholder(uint32_t tick);
    uint16_t screen_width() const { return 368; }
    uint16_t screen_height() const { return 448; }
    esp_lcd_panel_handle_t panel_handle() const { return panel_; }

private:
    bool init_framebuffers();
    void present_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void clear_screen(uint32_t rgb888);
    void fill_rect_mem(int x, int y, int w, int h, uint32_t rgb888);
    void draw_rect_outline(int x, int y, int w, int h, uint32_t rgb888, int thickness = 1);
    void draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, uint8_t scale = 1);
    void draw_text(int x, int y, const char* text, uint32_t fg, uint32_t bg, uint8_t scale = 1);
    void draw_title_block(const char* title);
    void draw_button(int x, int y, int w, int h, const char* label, bool active, uint8_t scale = 1);
    void draw_transport_buttons(bool is_playing, UiControl highlighted);
    void draw_progress_bar(int x, int y, int w, int h, float ratio, uint32_t fill, uint32_t track);
    static uint8_t glyph_row(char c, int row);

    esp_lcd_panel_io_handle_t io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    uint8_t* framebuffer_ = nullptr;
    uint8_t* flush_buf_ = nullptr;
    size_t framebuffer_bytes_ = 0;
    size_t flush_buf_bytes_ = 0;
};
