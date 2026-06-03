#include "TouchManager.hpp"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/task.h"

#define I2C_HOST       I2C_NUM_0
#define FT3168_ADDR    0x38
#define PIN_TOUCH_INT  GPIO_NUM_21

// FT3168 registers
#define REG_DEVICE_ID    0xA0
#define REG_FINGER_NUM   0x02
#define REG_X1_POSH      0x03
#define REG_X1_POSL      0x04
#define REG_Y1_POSH      0x05
#define REG_Y1_POSL      0x06
#define REG_POWER_MODE   0xA5

uint8_t TouchManager::i2c_read_reg(uint8_t reg)
{
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (FT3168_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (FT3168_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_HOST, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return data;
}

void TouchManager::i2c_write_reg(uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (FT3168_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_HOST, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
}

bool TouchManager::init(QueueHandle_t event_queue)
{
    event_queue_ = event_queue;

    ESP_LOGI("TOUCH", "Initializing FT3168 @ 0x%02X...", FT3168_ADDR);

    // Set active mode
    i2c_write_reg(REG_POWER_MODE, 0x01);
    vTaskDelay(pdMS_TO_TICKS(30));

    // Read device ID
    uint8_t dev_id = i2c_read_reg(REG_DEVICE_ID);
    ESP_LOGI("TOUCH", "FT3168 Device ID: 0x%02X (expected 0x03 for FT3168)", dev_id);

    initialized_ = true;
    ESP_LOGI("TOUCH", "FT3168 touch initialized OK");
    return true;
}

void TouchManager::deinit()
{
    initialized_ = false;
}

bool TouchManager::read_touch(TouchPoint& pt)
{
    if (!initialized_) return false;

    uint8_t finger_num = i2c_read_reg(REG_FINGER_NUM);
    if (finger_num == 0 || finger_num > 2) {
        pt.pressed = false;
        return false;
    }

    uint8_t xh = i2c_read_reg(REG_X1_POSH);
    uint8_t xl = i2c_read_reg(REG_X1_POSL);
    uint8_t yh = i2c_read_reg(REG_Y1_POSH);
    uint8_t yl = i2c_read_reg(REG_Y1_POSL);

    pt.x = ((xh & 0x0F) << 8) | xl;
    pt.y = ((yh & 0x0F) << 8) | yl;
    pt.pressed = true;
    return true;
}

void TouchManager::task_loop()
{
    TouchPoint pt;
    while (1) {
        if (read_touch(pt)) {
            ESP_LOGI("TOUCH", "Touch: x=%u y=%u", pt.x, pt.y);
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
