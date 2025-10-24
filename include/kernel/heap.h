#pragma once
#include "types.h"

/**
 * Allocation Header Structure
 * 
 * This header is prepended to every allocation made by kmalloc/kcalloc.
 * It enables:
 * - Corruption detection via magic number validation
 * - Accurate size tracking for debugging and statistics
 * - Fast determination of allocation source (slab vs heap)
 * - O(1) slab cache lookup for frees
 * 
 * Memory layout:
 *   [alloc_header_t][user data...]
 *                   ^
 *                   returned pointer
 */
typedef struct alloc_header {
    uint32_t magic;              // Magic number for validation (0xDEADBEEF)
    uint32_t size;               // Requested size (before alignment)
    uint16_t flags;              // Allocation source flags
    uint8_t slab_cache_index;    // Index into slab cache array (0-7)
    uint8_t padding;             // Padding for alignment
} alloc_header_t;

// Magic number for allocation header validation
#define ALLOC_MAGIC 0xDEADBEEF

// Allocation source flags
#define ALLOC_FROM_SLAB 0x01    // Allocated from slab cache
#define ALLOC_FROM_HEAP 0x02    // Allocated from heap

// Slab cache index values (for ALLOC_FROM_SLAB allocations)
#define SLAB_CACHE_16    0
#define SLAB_CACHE_32    1
#define SLAB_CACHE_64    2
#define SLAB_CACHE_128   3
#define SLAB_CACHE_256   4
#define SLAB_CACHE_512   5
#define SLAB_CACHE_1024  6
#define SLAB_CACHE_2048  7
#define SLAB_CACHE_NONE  0xFF   // Not from slab cache

// Public heap interface
void heap_init(uint64_t start, uint64_t size);
void heap_enable_slab(void);
void *kmalloc(size_t size);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);

// Allocation with GFP flags
void *kmalloc_flags(size_t size, uint32_t flags);
void *kcalloc_flags(size_t num, size_t size, uint32_t flags);
