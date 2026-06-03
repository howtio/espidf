#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstddef>
#include <cstdint>

struct SystemStatus {
    bool is_charging;
    bool usb_connected;
    uint8_t battery_percent;
    float battery_voltage;
    size_t sram_free;
    size_t sram_min_free;
    size_t psram_free;
    size_t psram_min_free;
    float pcm_water_level;
    int gif_fps;
    const char* gif_level;
};

class SystemMonitor {
public:
    static SystemMonitor& instance();

    bool init(QueueHandle_t ui_event_queue);
    void deinit();

    SystemStatus collect();
    void print_boot_banner();
    void print_status(const SystemStatus& st);
    void task_loop();

    static void log(const char* level, const char* module, const char* fmt, ...);
};
