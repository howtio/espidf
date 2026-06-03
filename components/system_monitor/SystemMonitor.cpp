#include "SystemMonitor.hpp"
#include "MemoryPool.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

SystemMonitor& SystemMonitor::instance() {
    static SystemMonitor mon;
    return mon;
}

bool SystemMonitor::init(QueueHandle_t ui_event_queue) {
    (void)ui_event_queue;
    ESP_LOGI("SYS", "System monitor ready");
    return true;
}
void SystemMonitor::deinit() {}

SystemStatus SystemMonitor::collect() {
    SystemStatus st = {};
    MemoryPool& pool = MemoryPool::instance();
    st.is_charging = false;
    st.usb_connected = true;
    st.battery_percent = 0;
    st.battery_voltage = 0.0f;
    st.sram_free = pool.sram_free();
    st.sram_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    st.psram_free = pool.psram_free();
    st.psram_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    st.pcm_water_level = 0.0f;
    st.gif_fps = 0;
    st.gif_level = "OFF";
    return st;
}

void SystemMonitor::print_boot_banner() {
    ESP_LOGI("SYS", "========================================");
    ESP_LOGI("SYS", "  ESP32 MP3 Player v0.1.0");
    ESP_LOGI("SYS", "  HW: ESP32-S3-Touch-AMOLED-1.8");
    ESP_LOGI("SYS", "  CPU: 240MHz | PSRAM: 8MB | Flash: 16MB");
    ESP_LOGI("SYS", "  LCD: 368x448 AMOLED QSPI");
    ESP_LOGI("SYS", "========================================");
}

void SystemMonitor::print_status(const SystemStatus& st) {
    ESP_LOGI("MEM", "SRAM free:%uKB min:%uKB | PSRAM free:%uKB min:%uKB",
             (unsigned)(st.sram_free / 1024),
             (unsigned)(st.sram_min_free / 1024),
             (unsigned)(st.psram_free / 1024),
             (unsigned)(st.psram_min_free / 1024));
    ESP_LOGI("PWR", "Battery:%u%% | charging:%s | voltage:%.2fV",
             st.battery_percent,
             st.is_charging ? "yes" : "no",
             st.battery_voltage);
}

void SystemMonitor::task_loop() {
    while (true) {
        print_status(collect());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void SystemMonitor::log(const char* level, const char* module, const char* fmt, ...) {
    (void)level;
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ESP_LOGI(module, "%s", buf);
}
