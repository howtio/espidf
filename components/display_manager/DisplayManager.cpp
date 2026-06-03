#include "DisplayManager.hpp"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_HOST SPI2_HOST
#define I2C_HOST I2C_NUM_0

#define PIN_LCD_CS   GPIO_NUM_12
#define PIN_LCD_PCLK GPIO_NUM_11
#define PIN_LCD_D0   GPIO_NUM_4
#define PIN_LCD_D1   GPIO_NUM_5
#define PIN_LCD_D2   GPIO_NUM_6
#define PIN_LCD_D3   GPIO_NUM_7

#define PIN_I2C_SCL GPIO_NUM_14
#define PIN_I2C_SDA GPIO_NUM_15

#define IOEXP_ADDR        0x20
#define IOEXP_REG_OUTPUT  0x01
#define IOEXP_REG_CONFIG  0x03
#define IOEXP_BIT_LCD_RST 0
#define IOEXP_BIT_PWR_EN  1
#define IOEXP_BIT_TP_RST  2

#define LCD_H_RES 368
#define LCD_V_RES 448
#define LCD_BPP   24

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
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

bool DisplayManager::init()
{

    // IO expander (TCAL9534 @ 0x20) for LCD/TP reset control
    ESP_LOGI("LCD", "Initializing IO expander (TCAL9534 @ 0x20)...");
    {
        auto ioexp_write = [](uint8_t reg, uint8_t val) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (IOEXP_ADDR << 1) | I2C_MASTER_WRITE, true);
            i2c_master_write_byte(cmd, reg, true);
            i2c_master_write_byte(cmd, val, true);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(I2C_HOST, cmd, pdMS_TO_TICKS(100));
            i2c_cmd_link_delete(cmd);
            return ret;
        };
        // Set bits 0,1,2,7 as outputs
        ioexp_write(IOEXP_REG_CONFIG, 0x78);  // bits 0,1,2,7 = output, others = input
        // Reset sequence: all low
        ioexp_write(IOEXP_REG_OUTPUT, 0x00);
        vTaskDelay(pdMS_TO_TICKS(100));
        // Release reset + enable SD: bits 0,1,2,7 high
        ioexp_write(IOEXP_REG_OUTPUT, (1 << IOEXP_BIT_LCD_RST) | (1 << IOEXP_BIT_PWR_EN) | (1 << IOEXP_BIT_TP_RST) | (1 << 7));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI("LCD", "IO expander OK, LCD/TP reset released");

    // QSPI bus (field order must match spi_bus_config_t)
    ESP_LOGI("LCD", "Initializing QSPI bus...");
    spi_bus_config_t bus_cfg = {
        .data0_io_num = PIN_LCD_D0,
        .data1_io_num = PIN_LCD_D1,
        .sclk_io_num = PIN_LCD_PCLK,
        .data2_io_num = PIN_LCD_D2,
        .data3_io_num = PIN_LCD_D3,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * LCD_BPP / 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // Panel IO (field order must match esp_lcd_panel_io_spi_config_t)
    ESP_LOGI("LCD", "Installing panel IO...");
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = { .quad_mode = true },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io_));

    // Panel
    ESP_LOGI("LCD", "Installing SH8601 driver...");
    sh8601_vendor_config_t vendor_cfg = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,  // Reset handled by IO expander
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = &vendor_cfg,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_, &panel_cfg, &panel_));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI("LCD", "Display initialized: %dx%d QSPI SH8601 OK", LCD_H_RES, LCD_V_RES);

    // Screen fill skipped - testing if LCD DMA interferes with I2S audio
    // Only draw a small indicator
    {
        uint8_t row_buf[LCD_H_RES * 3];
        memset(row_buf, 0x80, sizeof(row_buf));
        esp_lcd_panel_draw_bitmap(panel_, 0, 0, LCD_H_RES, 1, row_buf);
        ESP_LOGI("LCD", "Screen test line drawn");
    }
    return true;
}

void DisplayManager::deinit()
{
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
    if (panel_) {
        esp_lcd_panel_draw_bitmap(panel_, x, y, x + w, y + h, data);
    }
}

void* DisplayManager::lvgl_display_driver()
{
    return panel_;
}
