#pragma once
#include "../kernel/types.h"

// GFP (Get Free Pages) flags for memory allocation
// These flags control allocation behavior under memory pressure

// Basic allocation flags
#define GFP_KERNEL      0x00    // Normal kernel allocation, can sleep
#define GFP_ATOMIC      0x01    // Atomic allocation, cannot sleep (for interrupts)
#define GFP_NOWAIT      0x02    // Don't wait for memory, fail immediately
#define GFP_ZERO        0x04    // Zero the allocated memory
#define GFP_DMA         0x08    // Allocate from DMA-capable memory

// Zone modifiers
#define GFP_UNMOVABLE   0x00    // Allocate from unmovable zone (default)
#define GFP_RECLAIMABLE 0x10    // Allocate from reclaimable zone
#define GFP_MOVABLE     0x20    // Allocate from movable zone

// Compound flags for common use cases
#define GFP_KERNEL_ZERO (GFP_KERNEL | GFP_ZERO)
#define GFP_ATOMIC_ZERO (GFP_ATOMIC | GFP_ZERO)

// Helper macros to extract zone type from flags
#define GFP_ZONE_MASK   0x30
#define GFP_GET_ZONE(flags) \
    (((flags) & GFP_RECLAIMABLE) ? BUDDY_ZONE_RECLAIMABLE : \
     ((flags) & GFP_MOVABLE) ? BUDDY_ZONE_MOVABLE : \
     BUDDY_ZONE_UNMOVABLE)

// Memory allocation functions with flags
uint64_t buddy_alloc_pages_flags(uint32_t order, uint32_t flags);
void *kmalloc_flags(size_t size, uint32_t flags);
void *kcalloc_flags(size_t num, size_t size, uint32_t flags);

