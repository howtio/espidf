#include "SystemMonitor.hpp"
#include "esp_log.h"
#include <cstdio>
#include <cstdarg>

SystemMonitor& SystemMonitor::instance() {
    static SystemMonitor mon;
    return mon;
}

bool SystemMonitor::init(QueueHandle_t ui_event_queue) { return true; }
void SystemMonitor::deinit() {}

SystemStatus SystemMonitor::collect() {
    SystemStatus st = {};
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

void SystemMonitor::print_status(const SystemStatus& st) {}

void SystemMonitor::task_loop() {}

void SystemMonitor::log(const char* level, const char* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ESP_LOGI(module, "%s", buf);
}
