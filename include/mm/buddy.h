#pragma once
#include "../kernel/types.h"
#include "../kernel/spinlock.h"

#define BUDDY_MAX_ORDER 10
#define BUDDY_MIN_ORDER 0
#define BUDDY_PAGE_SIZE 4096

typedef struct buddy_block {
    struct buddy_block *next;
    struct buddy_block *prev;
} buddy_block_t;

typedef enum {
    BUDDY_ZONE_UNMOVABLE,
    BUDDY_ZONE_RECLAIMABLE,
    BUDDY_ZONE_MOVABLE,
    BUDDY_ZONE_COUNT
} buddy_zone_type_t;

typedef struct buddy_zone {
    buddy_block_t *free_lists[BUDDY_MAX_ORDER + 1];
    uint64_t free_counts[BUDDY_MAX_ORDER + 1];
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t base_address;
    uint8_t *allocation_map;
    uint64_t map_size;
    spinlock_t lock;
} buddy_zone_t;

void buddy_init(uint64_t memory_start, uint64_t memory_size);
uint64_t buddy_alloc_pages(uint32_t order, buddy_zone_type_t zone_type);
void buddy_free_pages(uint64_t address, uint32_t order);
uint64_t buddy_get_free_pages(void);
uint64_t buddy_get_total_pages(void);
void buddy_get_order_stats(uint32_t order, uint64_t *free_count);
void buddy_dump_stats(void);
void buddy_dump_zone(buddy_zone_type_t zone_type);

// Allocation with GFP flags
uint64_t buddy_alloc_pages_flags(uint32_t order, uint32_t flags);
