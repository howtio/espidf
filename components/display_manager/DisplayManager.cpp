#include "DisplayManager.hpp"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "board_config.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

constexpr spi_host_device_t kLcdHost = SPI2_HOST;
constexpr i2c_port_t kI2cHost = I2C_NUM_0;
constexpr uint8_t kIoExpRegOutput = 0x01;
constexpr uint8_t kIoExpRegConfig = 0x03;
constexpr uint16_t kChunkRows = 8;
constexpr uint32_t kBytesPerPixel = 3;
constexpr uint32_t kLcdBitsPerPixel = 24;
constexpr int kStatusBarY = 0;
constexpr int kStatusBarH = 32;
constexpr int kGifBoxX = 104;
constexpr int kGifBoxY = 56;
constexpr int kGifBoxSize = 160;
constexpr int kSongPanelX = 20;
constexpr int kSongPanelY = 242;
constexpr int kSongPanelW = 328;
constexpr int kSongPanelH = 74;
constexpr int kProgressX = 28;
constexpr int kProgressY = 332;
constexpr int kProgressW = 312;
constexpr int kProgressH = 16;
constexpr int kControlsY = 372;
constexpr int kButtonW = 86;
constexpr int kButtonH = 44;
constexpr int kButtonGap = 10;
constexpr int kButtonTouchPadX = 12;
constexpr int kButtonTouchPadY = 18;
constexpr int kAnimInset = 18;
constexpr int kAnimBarW = 18;

constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

constexpr uint32_t kBgColor = rgb(27, 9, 29);
constexpr uint32_t kPanelColor = rgb(51, 21, 48);
constexpr uint32_t kPanelBorder = rgb(255, 154, 206);
constexpr uint32_t kAccentColor = rgb(255, 108, 176);
constexpr uint32_t kAccentSoft = rgb(121, 61, 111);
constexpr uint32_t kTextMain = rgb(255, 246, 251);
constexpr uint32_t kTextDim = rgb(232, 194, 216);
constexpr uint32_t kGoodColor = rgb(255, 137, 196);
constexpr uint32_t kWarnColor = rgb(255, 204, 144);
constexpr uint32_t kTrackColor = rgb(88, 42, 79);
constexpr uint32_t kGifStageColor = rgb(255, 244, 249);

static const sh8601_lcd_init_cmd_t kLcdInitCmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

esp_err_t ioexp_write(uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_IO_EXPANDER << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(kI2cHost, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void store_rgb888(uint8_t* dst, uint32_t color)
{
    dst[0] = static_cast<uint8_t>((color >> 16) & 0xFF);
    dst[1] = static_cast<uint8_t>((color >> 8) & 0xFF);
    dst[2] = static_cast<uint8_t>(color & 0xFF);
}

}  // namespace

bool DisplayManager::init()
{
    ESP_LOGI("LCD", "[INF] Init display pipeline");
    ui_mutex_ = xSemaphoreCreateMutex();

    ESP_LOGI("LCD", "[INF] Init IO expander for LCD and touch reset");
    ESP_ERROR_CHECK(ioexp_write(kIoExpRegConfig, 0x78));
    ESP_ERROR_CHECK(ioexp_write(kIoExpRegOutput, 0x00));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(ioexp_write(kIoExpRegOutput,
                                (1 << IOEXP_BIT_LCD_RST) |
                                (1 << IOEXP_BIT_DSI_PWR_EN) |
                                (1 << IOEXP_BIT_TOUCH_RST) |
                                (1 << IOEXP_BIT_SD_CS)));
    vTaskDelay(pdMS_TO_TICKS(100));

    spi_bus_config_t bus_cfg = {};
    bus_cfg.data0_io_num = BOARD_LCD_DATA0_IO;
    bus_cfg.data1_io_num = BOARD_LCD_DATA1_IO;
    bus_cfg.sclk_io_num = BOARD_LCD_PCLK_IO;
    bus_cfg.data2_io_num = BOARD_LCD_DATA2_IO;
    bus_cfg.data3_io_num = BOARD_LCD_DATA3_IO;
    bus_cfg.data4_io_num = -1;
    bus_cfg.data5_io_num = -1;
    bus_cfg.data6_io_num = -1;
    bus_cfg.data7_io_num = -1;
    bus_cfg.max_transfer_sz = BOARD_LCD_H_RES * kChunkRows * kBytesPerPixel;
    ESP_LOGI("LCD", "[INF] Init QSPI bus: %ux%u chunk=%u rows",
             BOARD_LCD_H_RES,
             BOARD_LCD_V_RES,
             kChunkRows);
    ESP_ERROR_CHECK(spi_bus_initialize(kLcdHost, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = BOARD_LCD_CS_IO;
    io_cfg.dc_gpio_num = -1;
    io_cfg.spi_mode = 0;
    io_cfg.pclk_hz = 40 * 1000 * 1000;
    io_cfg.trans_queue_depth = 10;
    io_cfg.lcd_cmd_bits = 32;
    io_cfg.lcd_param_bits = 8;
    io_cfg.flags.quad_mode = true;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)kLcdHost, &io_cfg, &io_));

    sh8601_vendor_config_t vendor_cfg = {
        .init_cmds = kLcdInitCmds,
        .init_cmds_size = sizeof(kLcdInitCmds) / sizeof(kLcdInitCmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = -1;
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
    panel_cfg.bits_per_pixel = kLcdBitsPerPixel;
    panel_cfg.vendor_config = &vendor_cfg;
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_, &panel_cfg, &panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    if (!init_framebuffers()) {
        ESP_LOGE("LCD", "[ERR] Framebuffer allocation failed");
        return false;
    }

    render_static_ui();
    update_now_playing("WAITING FOR PLAYLIST", 0, 0);
    update_status_bar(0, false, 0, 0, 0.0f);
    update_playback_meter(0.0f, false);

    ESP_LOGI("LCD", "[INF] Display ready: SH8601 %ux%u",
             BOARD_LCD_H_RES,
             BOARD_LCD_V_RES);
    return true;
}

bool DisplayManager::init_framebuffers()
{
    framebuffer_bytes_ = BOARD_LCD_H_RES * BOARD_LCD_V_RES * kBytesPerPixel;
    flush_buf_bytes_ = BOARD_LCD_H_RES * kChunkRows * kBytesPerPixel;
    framebuffer_ = static_cast<uint8_t*>(heap_caps_malloc(framebuffer_bytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    flush_buf_ = static_cast<uint8_t*>(heap_caps_malloc(flush_buf_bytes_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    ESP_LOGI("LCD", "[INF] Framebuffer alloc: fb=%u bytes (PSRAM) flush=%u bytes (SRAM)",
             static_cast<unsigned>(framebuffer_bytes_),
             static_cast<unsigned>(flush_buf_bytes_));
    return framebuffer_ && flush_buf_;
}

void DisplayManager::deinit()
{
    if (ui_mutex_) {
        vSemaphoreDelete(ui_mutex_);
        ui_mutex_ = nullptr;
    }
    if (framebuffer_) {
        heap_caps_free(framebuffer_);
        framebuffer_ = nullptr;
    }
    if (flush_buf_) {
        heap_caps_free(flush_buf_);
        flush_buf_ = nullptr;
    }
    if (panel_) {
        esp_lcd_panel_disp_on_off(panel_, false);
        esp_lcd_panel_del(panel_);
        panel_ = nullptr;
    }
    if (io_) {
        esp_lcd_panel_io_del(io_);
        io_ = nullptr;
    }
}

void DisplayManager::flush_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data)
{
    if (!panel_) {
        return;
    }
    esp_lcd_panel_draw_bitmap(panel_, x, y, x + w, y + h, data);
}

void* DisplayManager::lvgl_display_driver()
{
    return panel_;
}

void DisplayManager::render_static_ui()
{
    if (!framebuffer_) {
        return;
    }
    if (ui_mutex_ && xSemaphoreTake(ui_mutex_, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }

    ESP_LOGI("LCD", "[INF] Render static UI scaffold");
    clear_screen(kBgColor);

    fill_rect_mem(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, kBgColor);
    fill_rect_mem(0, kStatusBarY, BOARD_LCD_H_RES, kStatusBarH, kPanelColor);
    fill_rect_mem(kGifBoxX, kGifBoxY, kGifBoxSize, kGifBoxSize, kPanelColor);
    draw_rect_outline(kGifBoxX, kGifBoxY, kGifBoxSize, kGifBoxSize, kPanelBorder, 2);
    draw_rect_outline(kGifBoxX + 12, kGifBoxY + 12, kGifBoxSize - 24, kGifBoxSize - 24, kAccentSoft, 1);
    fill_rect_mem(kGifBoxX + 8, kGifBoxY + 8, kGifBoxSize - 16, kGifBoxSize - 16, kGifStageColor);
    draw_text(kGifBoxX + 40, kGifBoxY + 48, "PET UI", kAccentColor, kGifStageColor, 2);
    draw_text(kGifBoxX + 28, kGifBoxY + 102, "GIF FROM SD", kTextDim, kGifStageColor, 1);

    fill_rect_mem(kSongPanelX, kSongPanelY, kSongPanelW, kSongPanelH, kPanelColor);
    draw_rect_outline(kSongPanelX, kSongPanelY, kSongPanelW, kSongPanelH, kPanelBorder, 2);

    draw_text(20, 40, "ESP32 MP3 PLAYER", kAccentColor, kBgColor, 2);
    draw_text(20, 60, "PINK UI / GIF FROM FLASH", kTextDim, kBgColor, 1);

    draw_progress_bar(kProgressX, kProgressY, kProgressW, kProgressH, 0.0f, kGoodColor, kTrackColor);
    draw_text(kProgressX, kProgressY - 18, "TRACK PROGRESS", kTextDim, kBgColor, 1);
    draw_text(kProgressX + kProgressW - 54, kProgressY - 18, "0%", kTextDim, kBgColor, 1);

    draw_transport_buttons(false, UiControl::None);

    present_area(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    if (ui_mutex_) {
        xSemaphoreGive(ui_mutex_);
    }
}

void DisplayManager::update_status_bar(uint8_t battery_percent,
                                       bool is_charging,
                                       size_t sram_free_bytes,
                                       size_t psram_free_bytes,
                                       float pcm_water_level)
{
    if (ui_mutex_ && xSemaphoreTake(ui_mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    fill_rect_mem(0, kStatusBarY, BOARD_LCD_H_RES, kStatusBarH, kPanelColor);

    char left[32];
    std::snprintf(left, sizeof(left), "BAT %3u%% %s",
                  static_cast<unsigned>(battery_percent),
                  is_charging ? "CHG" : "DIS");
    draw_text(12, 11, left, kTextMain, kPanelColor, 1);

    char middle[32];
    std::snprintf(middle, sizeof(middle), "SRAM %3uKB",
                  static_cast<unsigned>(sram_free_bytes / 1024));
    draw_text(126, 11, middle, kTextMain, kPanelColor, 1);

    char right[40];
    std::snprintf(right, sizeof(right), "PSRAM %4uKB",
                  static_cast<unsigned>(psram_free_bytes / 1024));
    draw_text(236, 11, right, kTextMain, kPanelColor, 1);

    char water[16];
    const int water_percent = static_cast<int>(std::max(0.0f, std::min(100.0f, pcm_water_level * 100.0f)));
    std::snprintf(water, sizeof(water), "%2d%%", water_percent);
    draw_text(318, 11, water, water_percent < 30 ? kWarnColor : kGoodColor, kPanelColor, 1);

    present_area(0, kStatusBarY, BOARD_LCD_H_RES, kStatusBarH);
    ESP_LOGI("LCD", "[INF] Status bar updated: battery=%u%% pcm=%d%%",
             static_cast<unsigned>(battery_percent),
             water_percent);
    if (ui_mutex_) {
        xSemaphoreGive(ui_mutex_);
    }
}

void DisplayManager::update_now_playing(const char* title, size_t index, size_t total)
{
    if (ui_mutex_ && xSemaphoreTake(ui_mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    fill_rect_mem(kSongPanelX + 2, kSongPanelY + 2, kSongPanelW - 4, kSongPanelH - 4, kPanelColor);

    char header[32];
    if (total == 0) {
        std::snprintf(header, sizeof(header), "TRACK -- / --");
    } else {
        std::snprintf(header, sizeof(header), "TRACK %02u / %02u",
                      static_cast<unsigned>(index + 1),
                      static_cast<unsigned>(total));
    }
    draw_text(kSongPanelX + 14, kSongPanelY + 12, header, kAccentColor, kPanelColor, 1);
    draw_title_block(title ? title : "NO TRACK");
    present_area(kSongPanelX, kSongPanelY, kSongPanelW, kSongPanelH);
    ESP_LOGI("LCD", "[INF] Now playing updated: %s", title ? title : "NO TRACK");
    if (ui_mutex_) {
        xSemaphoreGive(ui_mutex_);
    }
}

void DisplayManager::update_playback_meter(float ratio, bool is_playing)
{
    (void)is_playing;
    if (ui_mutex_ && xSemaphoreTake(ui_mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    const float clamped = std::max(0.0f, std::min(1.0f, ratio));
    fill_rect_mem(0, kProgressY - 22, BOARD_LCD_H_RES, 38, kBgColor);

    draw_text(kProgressX, kProgressY - 18, "TRACK PROGRESS", kTextMain, kBgColor, 1);
    char water[20];
    std::snprintf(water, sizeof(water), "%3u%%",
                  static_cast<unsigned>(clamped * 100.0f));
    draw_text(kProgressX + kProgressW - 54, kProgressY - 18, water, kTextMain, kBgColor, 1);
    draw_progress_bar(kProgressX, kProgressY, kProgressW, kProgressH, clamped, kGoodColor, kTrackColor);
    present_area(0, kProgressY - 22, BOARD_LCD_H_RES, 38);
    if (ui_mutex_) {
        xSemaphoreGive(ui_mutex_);
    }
}

void DisplayManager::update_transport_controls(bool is_playing, UiControl highlighted)
{
    if (ui_mutex_ && xSemaphoreTake(ui_mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    fill_rect_mem(0, kControlsY, BOARD_LCD_H_RES, kButtonH + 4, kBgColor);
    draw_transport_buttons(is_playing, highlighted);
    present_area(0, kControlsY, BOARD_LCD_H_RES, kButtonH + 4);
    if (ui_mutex_) {
        xSemaphoreGive(ui_mutex_);
    }
}

UiControl DisplayManager::hit_test_control(uint16_t x, uint16_t y) const
{
    const UiControl controls[] = {
        UiControl::Prev,
        UiControl::PlayPause,
        UiControl::Next,
    };
    for (UiControl control : controls) {
        int bx = 0;
        int by = 0;
        int bw = 0;
        int bh = 0;
        if (!control_bounds(control, bx, by, bw, bh)) {
            continue;
        }
        const int hit_x0 = bx - 8;
        const int hit_y0 = by - 6;
        const int hit_x1 = bx + bw + 8;
        const int hit_y1 = by + bh + 6;
        if (x >= hit_x0 && x <= hit_x1 && y >= hit_y0 && y <= hit_y1) {
            return control;
        }
    }
    return UiControl::None;
}

void DisplayManager::animate_gif_placeholder(uint32_t tick)
{
    if (!animation_enabled_) {
        return;
    }
    if (ui_mutex_ && xSemaphoreTake(ui_mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }
    const int inner_x = kGifBoxX + kAnimInset;
    const int inner_y = kGifBoxY + kAnimInset;
    const int inner_w = kGifBoxSize - (kAnimInset * 2);
    const int inner_h = kGifBoxSize - (kAnimInset * 2);
    fill_rect_mem(inner_x, inner_y, inner_w, inner_h, kPanelColor);

    for (int i = 0; i < 4; ++i) {
        const int phase = static_cast<int>((tick / 2) + (i * 7));
        const int height = 28 + ((phase % 9) * 8);
        const int x = inner_x + 12 + (i * 26);
        const int y = inner_y + inner_h - height - 14;
        const uint32_t color = (i % 2 == 0) ? kAccentColor : kGoodColor;
        fill_rect_mem(x, y, kAnimBarW, height, color);
    }

    draw_text(kGifBoxX + 48, kGifBoxY + 18, "PET", kTextMain, kPanelColor, 2);
    draw_text(kGifBoxX + 34, kGifBoxY + 128, "ANIMATING", kTextDim, kPanelColor, 1);
    present_area(kGifBoxX + 2, kGifBoxY + 2, kGifBoxSize - 4, kGifBoxSize - 4);
    if (ui_mutex_) {
        xSemaphoreGive(ui_mutex_);
    }
}

void DisplayManager::render_gif_frame_rgb565(const uint16_t* frame, uint16_t width, uint16_t height)
{
    if (!frame || width == 0 || height == 0) {
        return;
    }
    if (ui_mutex_ && xSemaphoreTake(ui_mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }

    const uint16_t draw_w = std::min<uint16_t>(width, kGifBoxSize);
    const uint16_t draw_h = std::min<uint16_t>(height, kGifBoxSize);
    const int start_x = kGifBoxX + ((kGifBoxSize - draw_w) / 2);
    const int start_y = kGifBoxY + ((kGifBoxSize - draw_h) / 2);

    fill_rect_mem(kGifBoxX + 2, kGifBoxY + 2, kGifBoxSize - 4, kGifBoxSize - 4, kGifStageColor);
    for (uint16_t y = 0; y < draw_h; ++y) {
        for (uint16_t x = 0; x < draw_w; ++x) {
            const uint16_t px = frame[(y * width) + x];
            const uint8_t r = static_cast<uint8_t>(((px >> 11) & 0x1F) * 255 / 31);
            const uint8_t g = static_cast<uint8_t>(((px >> 5) & 0x3F) * 255 / 63);
            const uint8_t b = static_cast<uint8_t>((px & 0x1F) * 255 / 31);
            fill_rect_mem(start_x + x, start_y + y, 1, 1, rgb(r, g, b));
        }
    }
    draw_rect_outline(kGifBoxX, kGifBoxY, kGifBoxSize, kGifBoxSize, kPanelBorder, 2);
    present_area(kGifBoxX, kGifBoxY, kGifBoxSize, kGifBoxSize);
    if (ui_mutex_) {
        xSemaphoreGive(ui_mutex_);
    }
}

void DisplayManager::present_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (!panel_ || !framebuffer_ || !flush_buf_ || w == 0 || h == 0) {
        return;
    }

    // SH8601 requires draw areas aligned to even boundaries.
    const uint16_t x0 = static_cast<uint16_t>(x & ~1U);
    const uint16_t y0 = static_cast<uint16_t>(y & ~1U);
    const uint16_t x2 = std::min<uint16_t>(static_cast<uint16_t>((x + w + 1) & ~1U), BOARD_LCD_H_RES);
    const uint16_t y2 = std::min<uint16_t>(static_cast<uint16_t>((y + h + 1) & ~1U), BOARD_LCD_V_RES);
    if (x2 <= x0 || y2 <= y0) {
        return;
    }
    const uint16_t clipped_w = x2 - x0;

    for (uint16_t row = y0; row < y2; row += kChunkRows) {
        const uint16_t rows_this_time = std::min<uint16_t>(kChunkRows, y2 - row);
        for (uint16_t r = 0; r < rows_this_time; ++r) {
            const uint8_t* src = framebuffer_ + (((row + r) * BOARD_LCD_H_RES) + x0) * kBytesPerPixel;
            uint8_t* dst = flush_buf_ + (r * clipped_w * kBytesPerPixel);
            std::memcpy(dst, src, clipped_w * kBytesPerPixel);
        }
        flush_area(x0, row, clipped_w, rows_this_time, flush_buf_);
    }
}

void DisplayManager::clear_screen(uint32_t rgb888)
{
    fill_rect_mem(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, rgb888);
}

void DisplayManager::fill_rect_mem(int x, int y, int w, int h, uint32_t rgb888)
{
    if (!framebuffer_) {
        return;
    }
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min<int>(BOARD_LCD_H_RES, x + w);
    const int y1 = std::min<int>(BOARD_LCD_V_RES, y + h);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    for (int row = y0; row < y1; ++row) {
        uint8_t* dst = framebuffer_ + ((row * BOARD_LCD_H_RES) + x0) * kBytesPerPixel;
        for (int col = x0; col < x1; ++col) {
            store_rgb888(dst, rgb888);
            dst += kBytesPerPixel;
        }
    }
}

void DisplayManager::draw_rect_outline(int x, int y, int w, int h, uint32_t rgb888, int thickness)
{
    for (int i = 0; i < thickness; ++i) {
        fill_rect_mem(x + i, y + i, w - (i * 2), 1, rgb888);
        fill_rect_mem(x + i, y + h - 1 - i, w - (i * 2), 1, rgb888);
        fill_rect_mem(x + i, y + i, 1, h - (i * 2), rgb888);
        fill_rect_mem(x + w - 1 - i, y + i, 1, h - (i * 2), rgb888);
    }
}

void DisplayManager::draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, uint8_t scale)
{
    char normalized = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (int row = 0; row < 7; ++row) {
        const uint8_t bits = glyph_row(normalized, row);
        for (int col = 0; col < 5; ++col) {
            const bool on = bits & (1 << (4 - col));
            fill_rect_mem(x + (col * scale),
                          y + (row * scale),
                          scale,
                          scale,
                          on ? fg : bg);
        }
    }
}

void DisplayManager::draw_text(int x, int y, const char* text, uint32_t fg, uint32_t bg, uint8_t scale)
{
    if (!text) {
        return;
    }
    int cursor = x;
    while (*text) {
        draw_char(cursor, y, *text, fg, bg, scale);
        cursor += 6 * scale;
        ++text;
    }
}

void DisplayManager::draw_title_block(const char* title)
{
    const int line_width_chars = 25;
    char normalized[64];
    std::memset(normalized, 0, sizeof(normalized));

    size_t out = 0;
    for (size_t i = 0; title[i] != '\0' && out < sizeof(normalized) - 1; ++i) {
        char c = title[i];
        if (std::islower(static_cast<unsigned char>(c))) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '.' || c == '-' || c == '_') {
            normalized[out++] = c;
        } else if (c == '/') {
            normalized[out++] = ' ';
        } else {
            normalized[out++] = ' ';
        }
    }

    char line1[32] = {};
    char line2[32] = {};
    std::snprintf(line1, sizeof(line1), "%.*s", line_width_chars, normalized);
    if (std::strlen(normalized) > static_cast<size_t>(line_width_chars)) {
        std::snprintf(line2, sizeof(line2), "%.*s", line_width_chars, normalized + line_width_chars);
    }

    draw_text(kSongPanelX + 14, kSongPanelY + 32, line1, kTextMain, kPanelColor, 2);
    if (line2[0] != '\0') {
        draw_text(kSongPanelX + 14, kSongPanelY + 50, line2, kTextMain, kPanelColor, 1);
    }
}

void DisplayManager::draw_button(int x, int y, int w, int h, const char* label, bool active, uint8_t scale)
{
    const uint32_t fill = active ? kAccentColor : kPanelColor;
    const uint32_t border = active ? kAccentColor : kPanelBorder;
    const uint32_t text = active ? kBgColor : kTextMain;
    fill_rect_mem(x, y, w, h, fill);
    draw_rect_outline(x, y, w, h, border, 2);
    const int label_len = static_cast<int>(std::strlen(label));
    const int text_w = label_len * 6 * scale;
    const int text_h = 7 * scale;
    const int text_x = x + std::max(8, (w - text_w) / 2);
    const int text_y = y + std::max(6, (h - text_h) / 2);
    draw_text(text_x, text_y, label, text, fill, scale);
}

void DisplayManager::draw_transport_buttons(bool is_playing, UiControl highlighted)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    control_bounds(UiControl::Prev, x, y, w, h);
    draw_button(x,
                y,
                w,
                h,
                "|<<",
                highlighted == UiControl::Prev,
                3);
    control_bounds(UiControl::PlayPause, x, y, w, h);
    draw_button(x,
                y,
                w,
                h,
                is_playing ? "||" : ">|",
                highlighted == UiControl::PlayPause,
                3);
    control_bounds(UiControl::Next, x, y, w, h);
    draw_button(x,
                y,
                w,
                h,
                ">>|",
                highlighted == UiControl::Next,
                3);
}

void DisplayManager::draw_progress_bar(int x, int y, int w, int h, float ratio, uint32_t fill, uint32_t track)
{
    draw_rect_outline(x, y, w, h, kPanelBorder, 1);
    fill_rect_mem(x + 1, y + 1, w - 2, h - 2, track);
    const int fill_w = static_cast<int>((w - 2) * std::max(0.0f, std::min(1.0f, ratio)));
    if (fill_w > 0) {
        fill_rect_mem(x + 1, y + 1, fill_w, h - 2, fill);
    }
}

bool DisplayManager::control_bounds(UiControl control, int& x, int& y, int& w, int& h) const
{
    const int total_controls_w = (kButtonW * 3) + (kButtonGap * 2);
    const int start_x = (BOARD_LCD_H_RES - total_controls_w) / 2;
    x = start_x;
    y = kControlsY;
    w = kButtonW;
    h = kButtonH;

    switch (control) {
        case UiControl::Prev:
            return true;
        case UiControl::PlayPause:
            x = start_x + kButtonW + kButtonGap;
            return true;
        case UiControl::Next:
            x = start_x + ((kButtonW + kButtonGap) * 2);
            return true;
        case UiControl::None:
        default:
            return false;
    }
}

uint8_t DisplayManager::glyph_row(char c, int row)
{
    switch (c) {
        case 'A': { static const uint8_t p[] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}; return p[row]; }
        case 'B': { static const uint8_t p[] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}; return p[row]; }
        case 'C': { static const uint8_t p[] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}; return p[row]; }
        case 'D': { static const uint8_t p[] = {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}; return p[row]; }
        case 'E': { static const uint8_t p[] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}; return p[row]; }
        case 'F': { static const uint8_t p[] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}; return p[row]; }
        case 'G': { static const uint8_t p[] = {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F}; return p[row]; }
        case 'H': { static const uint8_t p[] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}; return p[row]; }
        case 'I': { static const uint8_t p[] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}; return p[row]; }
        case 'J': { static const uint8_t p[] = {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}; return p[row]; }
        case 'K': { static const uint8_t p[] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}; return p[row]; }
        case 'L': { static const uint8_t p[] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}; return p[row]; }
        case 'M': { static const uint8_t p[] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}; return p[row]; }
        case 'N': { static const uint8_t p[] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}; return p[row]; }
        case 'O': { static const uint8_t p[] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}; return p[row]; }
        case 'P': { static const uint8_t p[] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}; return p[row]; }
        case 'Q': { static const uint8_t p[] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}; return p[row]; }
        case 'R': { static const uint8_t p[] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}; return p[row]; }
        case 'S': { static const uint8_t p[] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}; return p[row]; }
        case 'T': { static const uint8_t p[] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}; return p[row]; }
        case 'U': { static const uint8_t p[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}; return p[row]; }
        case 'V': { static const uint8_t p[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}; return p[row]; }
        case 'W': { static const uint8_t p[] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}; return p[row]; }
        case 'X': { static const uint8_t p[] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}; return p[row]; }
        case 'Y': { static const uint8_t p[] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}; return p[row]; }
        case 'Z': { static const uint8_t p[] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}; return p[row]; }
        case '0': { static const uint8_t p[] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}; return p[row]; }
        case '1': { static const uint8_t p[] = {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F}; return p[row]; }
        case '2': { static const uint8_t p[] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}; return p[row]; }
        case '3': { static const uint8_t p[] = {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E}; return p[row]; }
        case '4': { static const uint8_t p[] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}; return p[row]; }
        case '5': { static const uint8_t p[] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}; return p[row]; }
        case '6': { static const uint8_t p[] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}; return p[row]; }
        case '7': { static const uint8_t p[] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}; return p[row]; }
        case '8': { static const uint8_t p[] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}; return p[row]; }
        case '9': { static const uint8_t p[] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x1C}; return p[row]; }
        case '<': { static const uint8_t p[] = {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01}; return p[row]; }
        case '>': { static const uint8_t p[] = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10}; return p[row]; }
        case '|': { static const uint8_t p[] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}; return p[row]; }
        case '-': { static const uint8_t p[] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}; return p[row]; }
        case '.': { static const uint8_t p[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}; return p[row]; }
        case ':': { static const uint8_t p[] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}; return p[row]; }
        case '_': { static const uint8_t p[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}; return p[row]; }
        case '/': { static const uint8_t p[] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}; return p[row]; }
        case '%': { static const uint8_t p[] = {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}; return p[row]; }
        case ' ': default: return 0x00;
    }
}
