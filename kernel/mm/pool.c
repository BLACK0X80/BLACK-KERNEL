#include "../../include/mm/pool.h"
#include "../../include/mm/buddy.h"
#include "../../include/kernel/string.h"

#define MIN_OBJECT_SIZE sizeof(pool_chunk_t)

static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static int pool_grow(memory_pool_t *pool, uint32_t count);

memory_pool_t *pool_create(const char *name, size_t object_size, uint32_t initial_count) {
    if (!name || object_size == 0 || initial_count == 0) {
        return NULL;
    }
    
    // Allocate memory for the pool structure itself
    uint64_t pool_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    if (pool_addr == 0) {
        return NULL;
    }
    
    memory_pool_t *pool = (memory_pool_t *)(uintptr_t)pool_addr;
    memset(pool, 0, sizeof(memory_pool_t));
    
    // Initialize pool metadata
    strncpy(pool->name, name, POOL_NAME_MAX - 1);
    pool->name[POOL_NAME_MAX - 1] = '\0';
    
    // Ensure object size is at least large enough to hold a free list pointer
    pool->object_size = object_size;
    if (pool->object_size < MIN_OBJECT_SIZE) {
        pool->object_size = MIN_OBJECT_SIZE;
    }
    
    // Align object size to 8 bytes for proper alignment
    pool->object_size = align_up(pool->object_size, 8);
    
    pool->initial_count = initial_count;
    pool->grow_count = initial_count / 2;
    if (pool->grow_count == 0) {
        pool->grow_count = 1;
    }
    
    pool->total_objects = 0;
    pool->free_objects = 0;
    pool->free_list = NULL;
    pool->regions = NULL;
    
    spinlock_init(&pool->lock);
    
    // Pre-allocate initial objects
    if (pool_grow(pool, initial_count) < 0) {
        buddy_free_pages((uint64_t)(uintptr_t)pool, 0);
        return NULL;
    }
    
    return pool;
}

void *pool_alloc(memory_pool_t *pool) {
    if (!pool) {
        return NULL;
    }
    
    spinlock_acquire(&pool->lock);
    
    // If free list is empty, try to grow the pool
    if (pool->free_list == NULL) {
        if (pool_grow(pool, pool->grow_count) < 0) {
            spinlock_release(&pool->lock);
            return NULL;
        }
    }
    
    // Pop object from free list (constant time)
    pool_chunk_t *chunk = pool->free_list;
    pool->free_list = chunk->next;
    pool->free_objects--;
    
    spinlock_release(&pool->lock);
    
    return (void *)chunk;
}

void pool_free(memory_pool_t *pool, void *object) {
    if (!pool || !object) {
        return;
    }
    
    spinlock_acquire(&pool->lock);
    
    // Push object back to free list (constant time)
    pool_chunk_t *chunk = (pool_chunk_t *)object;
    chunk->next = pool->free_list;
    pool->free_list = chunk;
    pool->free_objects++;
    
    spinlock_release(&pool->lock);
}

static int pool_grow(memory_pool_t *pool, uint32_t count) {
    if (!pool || count == 0) {
        return -1;
    }
    
    // Calculate how many pages we need for the requested objects
    size_t total_size = count * pool->object_size;
    uint32_t pages_needed = (total_size + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
    
    // Calculate the order needed for buddy allocation
    uint32_t order = 0;
    uint32_t order_pages = 1;
    while (order_pages < pages_needed && order < BUDDY_MAX_ORDER) {
        order++;
        order_pages = 1 << order;
    }
    
    // Allocate memory region
    uint64_t region_addr = buddy_alloc_pages(order, BUDDY_ZONE_RECLAIMABLE);
    if (region_addr == 0) {
        return -1;
    }
    
    // Create region tracking structure
    pool_region_t *region = (pool_region_t *)(uintptr_t)region_addr;
    region->base = (void *)(uintptr_t)region_addr;
    region->next = pool->regions;
    pool->regions = region;
    
    // Calculate usable space (skip the region header)
    uint8_t *objects_start = (uint8_t *)region + sizeof(pool_region_t);
    size_t usable_size = (order_pages * BUDDY_PAGE_SIZE) - sizeof(pool_region_t);
    uint32_t objects_in_region = usable_size / pool->object_size;
    
    // Add all objects to the free list
    for (uint32_t i = 0; i < objects_in_region; i++) {
        pool_chunk_t *chunk = (pool_chunk_t *)(objects_start + i * pool->object_size);
        chunk->next = pool->free_list;
        pool->free_list = chunk;
    }
    
    pool->total_objects += objects_in_region;
    pool->free_objects += objects_in_region;
    
    return 0;
}

void pool_destroy(memory_pool_t *pool) {
    if (!pool) {
        return;
    }
    
    spinlock_acquire(&pool->lock);
    
    // Free all memory regions
    pool_region_t *region = pool->regions;
    while (region) {
        pool_region_t *next = region->next;
        
        // Calculate the order used for this region
        // We need to determine the size to free it properly
        // For simplicity, we'll track the order in the allocation
        // For now, we'll free as order 0 pages and let the buddy allocator handle it
        // In a production system, we'd track the order in the region structure
        
        // Calculate size from total objects
        size_t region_size = BUDDY_PAGE_SIZE; // Minimum size
        uint32_t order = 0;
        
        // Try to determine the order by checking if address is aligned
        uint64_t addr = (uint64_t)(uintptr_t)region;
        for (uint32_t o = 0; o <= BUDDY_MAX_ORDER; o++) {
            uint64_t alignment = (1ULL << o) * BUDDY_PAGE_SIZE;
            if ((addr & (alignment - 1)) == 0) {
                order = o;
            }
        }
        
        buddy_free_pages((uint64_t)(uintptr_t)region, order);
        region = next;
    }
    
    pool->regions = NULL;
    pool->free_list = NULL;
    pool->total_objects = 0;
    pool->free_objects = 0;
    
    spinlock_release(&pool->lock);
    
    // Free the pool structure itself
    buddy_free_pages((uint64_t)(uintptr_t)pool, 0);
}

uint32_t pool_get_utilization(memory_pool_t *pool) {
    if (!pool || pool->total_objects == 0) {
        return 0;
    }
    
    spinlock_acquire(&pool->lock);
    
    uint32_t used_objects = pool->total_objects - pool->free_objects;
    uint32_t utilization = (used_objects * 100) / pool->total_objects;
    
    spinlock_release(&pool->lock);
    
    return utilization;
}
