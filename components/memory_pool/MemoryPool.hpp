#pragma once
#include <cstddef>

class MemoryPool {
public:
    static MemoryPool& instance();

    bool init();
    void deinit();

    void* alloc_psram(size_t size);
    void* alloc_sram(size_t size);

    size_t psram_total_allocated() const;
    size_t sram_total_allocated() const;
    size_t psram_free() const;
    size_t sram_free() const;
};
