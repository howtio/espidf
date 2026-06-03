#pragma once
#include <cstdint>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"

class DisplayManager {
public:
    bool init();
    void deinit();
    void flush_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data);
    void* lvgl_display_driver();
    uint16_t screen_width() const { return 368; }
    uint16_t screen_height() const { return 448; }
    esp_lcd_panel_handle_t panel_handle() const { return panel_; }

private:
    esp_lcd_panel_io_handle_t io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
};
