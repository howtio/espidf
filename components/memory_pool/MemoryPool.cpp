#include "MemoryPool.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char* TAG = "MEM";

MemoryPool& MemoryPool::instance() {
    static MemoryPool pool;
    return pool;
}

bool MemoryPool::init() {
    psram_allocated_ = 0;
    sram_allocated_ = 0;
    ESP_LOGI(TAG, "Memory pool ready: SRAM free=%uKB, PSRAM free=%uKB",
             (unsigned)(sram_free() / 1024), (unsigned)(psram_free() / 1024));
    return true;
}

void MemoryPool::deinit() {}

void* MemoryPool::alloc_psram(size_t size) {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) {
        psram_allocated_ += size;
        ESP_LOGI(TAG, "PSRAM alloc: %u bytes (total=%uKB free=%uKB)",
                 (unsigned)size, (unsigned)(psram_allocated_ / 1024), (unsigned)(psram_free() / 1024));
    } else {
        ESP_LOGE(TAG, "PSRAM alloc failed: %u bytes", (unsigned)size);
    }
    return ptr;
}

void* MemoryPool::alloc_sram(size_t size) {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ptr) {
        sram_allocated_ += size;
        ESP_LOGI(TAG, "SRAM alloc: %u bytes (total=%uKB free=%uKB)",
                 (unsigned)size, (unsigned)(sram_allocated_ / 1024), (unsigned)(sram_free() / 1024));
    } else {
        ESP_LOGE(TAG, "SRAM alloc failed: %u bytes", (unsigned)size);
    }
    return ptr;
}

size_t MemoryPool::psram_total_allocated() const { return psram_allocated_; }
size_t MemoryPool::sram_total_allocated() const { return sram_allocated_; }
size_t MemoryPool::psram_free() const { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
size_t MemoryPool::sram_free() const { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
