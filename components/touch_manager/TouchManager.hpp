#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstdint>

struct TouchPoint {
    uint16_t x;
    uint16_t y;
    bool pressed;
};

class TouchManager {
public:
    bool init(QueueHandle_t event_queue);
    void deinit();
    void task_loop();
    bool read_touch(TouchPoint& pt);

private:
    QueueHandle_t event_queue_ = nullptr;
    bool initialized_ = false;
    uint8_t i2c_read_reg(uint8_t reg);
    void i2c_write_reg(uint8_t reg, uint8_t val);
};
