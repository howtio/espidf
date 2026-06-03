#include "MemoryPool.hpp"

MemoryPool& MemoryPool::instance() {
    static MemoryPool pool;
    return pool;
}

bool MemoryPool::init() { return true; }
void MemoryPool::deinit() {}
void* MemoryPool::alloc_psram(size_t size) { return nullptr; }
void* MemoryPool::alloc_sram(size_t size) { return nullptr; }
size_t MemoryPool::psram_total_allocated() const { return 0; }
size_t MemoryPool::sram_total_allocated() const { return 0; }
size_t MemoryPool::psram_free() const { return 8 * 1024 * 1024; }
size_t MemoryPool::sram_free() const { return 280 * 1024; }
