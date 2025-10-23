#pragma once
#include "types.h"

void heap_init(uint64_t start, uint64_t size);
void heap_enable_slab(void);
void *kmalloc(size_t size);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);

// Allocation with GFP flags
void *kmalloc_flags(size_t size, uint32_t flags);
void *kcalloc_flags(size_t num, size_t size, uint32_t flags);
