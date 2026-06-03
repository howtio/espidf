#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

enum class ButtonEvent { ShortPress, LongPress, DoubleClick };

class ButtonManager {
public:
    bool init(QueueHandle_t event_queue);
    void deinit();
    void task_loop();
};
