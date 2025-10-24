#include "../../include/mm/slab.h"
#include "../../include/mm/buddy.h"
#include "../../include/kernel/config.h"
#include "../../include/kernel/string.h"
#include "../../include/kernel/stdio.h"

#define CACHE_LINE_SIZE 64
#define MAX_CPUS 8

static slab_cache_t *g_cache_list = NULL;
static spinlock_t g_cache_list_lock;

void slab_init(void) {
    spinlock_init(&g_cache_list_lock);
    g_cache_list = NULL;
}

static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align) {
    // Validate parameters
    if (!name) {
        kprintf("[SLAB] ERROR: slab_cache_create called with NULL name\n");
        return NULL;
    }
    
    if (size == 0) {
        kprintf("[SLAB] ERROR: slab_cache_create called with zero size\n");
        return NULL;
    }
    
    if (size > BUDDY_PAGE_SIZE) {
        kprintf("[SLAB] ERROR: Object size %zu too large (max %u) for cache '%s'\n", 
                size, BUDDY_PAGE_SIZE, name);
        return NULL;
    }
    
    if (align == 0) {
        align = 8;
    }
    
    // Allocate memory for cache structure
    uint64_t cache_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    if (cache_addr == 0) {
        uint64_t free_pages = buddy_get_free_pages();
        uint64_t total_pages = buddy_get_total_pages();
        kprintf("[SLAB] ERROR: Failed to allocate cache structure for '%s'\n", name);
        kprintf("[SLAB] ERROR: Memory stats: %llu/%llu pages free\n", free_pages, total_pages);
        return NULL;
    }
    
    slab_cache_t *cache = (slab_cache_t *)(uintptr_t)cache_addr;
    memset(cache, 0, sizeof(slab_cache_t));
    
    strncpy(cache->name, name, SLAB_CACHE_NAME_MAX - 1);
    cache->name[SLAB_CACHE_NAME_MAX - 1] = '\0';
    
    cache->object_size = align_up(size, align);
    cache->align = align;
    
    size_t slab_size = BUDDY_PAGE_SIZE;
    size_t usable_size = slab_size - sizeof(slab_t);
    cache->objects_per_slab = usable_size / cache->object_size;
    
    if (cache->objects_per_slab == 0) {
        cache->objects_per_slab = 1;
    }
    
    cache->color_next = 0;
    cache->slabs_full = NULL;
    cache->slabs_partial = NULL;
    cache->slabs_free = NULL;
    cache->total_allocations = 0;
    cache->total_frees = 0;
    cache->cache_hits = 0;
    
    spinlock_init(&cache->lock);
    
    for (int i = 0; i < MAX_CPUS; i++) {
        cache->cpu_caches[i].objects = NULL;
        cache->cpu_caches[i].available = 0;
        cache->cpu_caches[i].limit = SLAB_CPU_CACHE_SIZE;
    }
    
    spinlock_acquire(&g_cache_list_lock);
    cache->next = g_cache_list;
    g_cache_list = cache;
    spinlock_release(&g_cache_list_lock);
    
    return cache;
}

void slab_cache_destroy(slab_cache_t *cache) {
    if (!cache) {
        return;
    }
    
    spinlock_acquire(&g_cache_list_lock);
    slab_cache_t **current = &g_cache_list;
    while (*current) {
        if (*current == cache) {
            *current = cache->next;
            break;
        }
        current = &(*current)->next;
    }
    spinlock_release(&g_cache_list_lock);
    
    spinlock_acquire(&cache->lock);
    
    slab_t *slab = cache->slabs_full;
    while (slab) {
        slab_t *next = slab->next;
        buddy_free_pages((uint64_t)(uintptr_t)slab, 0);
        slab = next;
    }
    
    slab = cache->slabs_partial;
    while (slab) {
        slab_t *next = slab->next;
        buddy_free_pages((uint64_t)(uintptr_t)slab, 0);
        slab = next;
    }
    
    slab = cache->slabs_free;
    while (slab) {
        slab_t *next = slab->next;
        buddy_free_pages((uint64_t)(uintptr_t)slab, 0);
        slab = next;
    }
    
    for (int i = 0; i < MAX_CPUS; i++) {
        if (cache->cpu_caches[i].objects) {
            buddy_free_pages((uint64_t)(uintptr_t)cache->cpu_caches[i].objects, 0);
        }
    }
    
    spinlock_release(&cache->lock);
    
    buddy_free_pages((uint64_t)(uintptr_t)cache, 0);
}

static slab_t *slab_create(slab_cache_t *cache) {
    uint64_t slab_addr = buddy_alloc_pages(0, BUDDY_ZONE_RECLAIMABLE);
    if (slab_addr == 0) {
        return NULL;
    }
    
    slab_t *slab = (slab_t *)(uintptr_t)slab_addr;
    slab->next = NULL;
    slab->in_use = 0;
    slab->total_objects = cache->objects_per_slab;
    
    size_t color_offset = (cache->color_next * CACHE_LINE_SIZE) % 
                          (BUDDY_PAGE_SIZE - sizeof(slab_t) - 
                           (cache->objects_per_slab * cache->object_size));
    cache->color_next = (cache->color_next + 1) % 8;
    
    slab->objects = (uint8_t *)slab + sizeof(slab_t) + color_offset;
    
    slab->free_list = NULL;
    for (uint32_t i = 0; i < cache->objects_per_slab; i++) {
        void **obj = (void **)(slab->objects + i * cache->object_size);
        *obj = slab->free_list;
        slab->free_list = obj;
    }
    
    return slab;
}

static void *slab_alloc_from_slab(slab_cache_t *cache, slab_t *slab) {
    if (!slab || !slab->free_list) {
        return NULL;
    }
    
    void *obj = slab->free_list;
    slab->free_list = *(void **)obj;
    slab->in_use++;
    
    return obj;
}

static void slab_move_to_list(slab_t **from_list, slab_t **to_list, slab_t *slab) {
    if (*from_list == slab) {
        *from_list = slab->next;
    } else {
        slab_t *current = *from_list;
        while (current && current->next != slab) {
            current = current->next;
        }
        if (current) {
            current->next = slab->next;
        }
    }
    
    slab->next = *to_list;
    *to_list = slab;
}

void *slab_alloc(slab_cache_t *cache) {
    // Validate cache pointer
    if (!cache) {
        kprintf("[SLAB] ERROR: slab_alloc called with NULL cache\n");
        return NULL;
    }
    
    int cpu_id = 0;
    slab_cpu_cache_t *cpu_cache = &cache->cpu_caches[cpu_id];
    
    if (cpu_cache->available > 0) {
        cpu_cache->available--;
        void *obj = cpu_cache->objects[cpu_cache->available];
        cache->cache_hits++;
        cache->total_allocations++;
        return obj;
    }
    
    spinlock_acquire(&cache->lock);
    
    void *obj = NULL;
    slab_t *slab = cache->slabs_partial;
    
    if (slab) {
        obj = slab_alloc_from_slab(cache, slab);
        
        if (slab->in_use == slab->total_objects) {
            slab_move_to_list(&cache->slabs_partial, &cache->slabs_full, slab);
        }
    } else {
        slab = cache->slabs_free;
        if (slab) {
            cache->slabs_free = slab->next;
            slab->next = cache->slabs_partial;
            cache->slabs_partial = slab;
            
            obj = slab_alloc_from_slab(cache, slab);
            
            if (slab->in_use == slab->total_objects) {
                slab_move_to_list(&cache->slabs_partial, &cache->slabs_full, slab);
            }
        } else {
            slab = slab_create(cache);
            if (slab) {
                slab->next = cache->slabs_partial;
                cache->slabs_partial = slab;
                
                obj = slab_alloc_from_slab(cache, slab);
                
                if (slab->in_use == slab->total_objects) {
                    slab_move_to_list(&cache->slabs_partial, &cache->slabs_full, slab);
                }
            }
        }
    }
    
    if (obj) {
        cache->total_allocations++;
    } else {
        DEBUG_PRINT(SLAB, "Failed to allocate from cache '%s' after creating new slab\n", 
                    cache->name);
        kprintf("[SLAB] ERROR: Failed to allocate object from cache '%s'\n", cache->name);
    }
    
    spinlock_release(&cache->lock);
    
    return obj;
}

static slab_t *slab_find_for_object(slab_cache_t *cache, void *object) {
    uint64_t obj_addr = (uint64_t)(uintptr_t)object;
    
    slab_t *slab = cache->slabs_full;
    while (slab) {
        uint64_t slab_addr = (uint64_t)(uintptr_t)slab;
        if (obj_addr >= slab_addr && obj_addr < slab_addr + BUDDY_PAGE_SIZE) {
            return slab;
        }
        slab = slab->next;
    }
    
    slab = cache->slabs_partial;
    while (slab) {
        uint64_t slab_addr = (uint64_t)(uintptr_t)slab;
        if (obj_addr >= slab_addr && obj_addr < slab_addr + BUDDY_PAGE_SIZE) {
            return slab;
        }
        slab = slab->next;
    }
    
    return NULL;
}

static void slab_free_to_slab(slab_cache_t *cache, slab_t *slab, void *object) {
    void **obj = (void **)object;
    *obj = slab->free_list;
    slab->free_list = obj;
    slab->in_use--;
}

void slab_free(slab_cache_t *cache, void *object) {
    // Validate parameters
    if (!cache) {
        kprintf("[SLAB] ERROR: slab_free called with NULL cache\n");
        return;
    }
    
    if (!object) {
        kprintf("[SLAB] ERROR: slab_free called with NULL object for cache '%s'\n", cache->name);
        return;
    }
    
    int cpu_id = 0;
    slab_cpu_cache_t *cpu_cache = &cache->cpu_caches[cpu_id];
    
    if (cpu_cache->available < cpu_cache->limit) {
        if (!cpu_cache->objects) {
            uint64_t cache_array_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
            if (cache_array_addr) {
                cpu_cache->objects = (void **)(uintptr_t)cache_array_addr;
            }
        }
        
        if (cpu_cache->objects) {
            cpu_cache->objects[cpu_cache->available] = object;
            cpu_cache->available++;
            cache->total_frees++;
            return;
        }
    }
    
    spinlock_acquire(&cache->lock);
    
    slab_t *slab = slab_find_for_object(cache, object);
    if (slab) {
        int was_full = (slab->in_use == slab->total_objects);
        
        slab_free_to_slab(cache, slab, object);
        
        if (was_full) {
            slab_move_to_list(&cache->slabs_full, &cache->slabs_partial, slab);
        } else if (slab->in_use == 0) {
            slab_move_to_list(&cache->slabs_partial, &cache->slabs_free, slab);
        }
        
        cache->total_frees++;
    } else {
        kprintf("[SLAB] WARNING: Freeing object %p not found in cache '%s'\n", 
                object, cache->name);
    }
    
    spinlock_release(&cache->lock);
}

static int slab_cpu_cache_refill(slab_cache_t *cache, int cpu_id) {
    slab_cpu_cache_t *cpu_cache = &cache->cpu_caches[cpu_id];
    
    if (!cpu_cache->objects) {
        uint64_t cache_array_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
        if (cache_array_addr == 0) {
            return -1;
        }
        cpu_cache->objects = (void **)(uintptr_t)cache_array_addr;
    }
    
    spinlock_acquire(&cache->lock);
    
    uint32_t batch_size = cpu_cache->limit / 2;
    if (batch_size == 0) {
        batch_size = 1;
    }
    
    uint32_t refilled = 0;
    while (refilled < batch_size && cpu_cache->available < cpu_cache->limit) {
        void *obj = NULL;
        slab_t *slab = cache->slabs_partial;
        
        if (slab) {
            obj = slab_alloc_from_slab(cache, slab);
            if (slab->in_use == slab->total_objects) {
                slab_move_to_list(&cache->slabs_partial, &cache->slabs_full, slab);
            }
        } else {
            slab = cache->slabs_free;
            if (slab) {
                cache->slabs_free = slab->next;
                slab->next = cache->slabs_partial;
                cache->slabs_partial = slab;
                obj = slab_alloc_from_slab(cache, slab);
            } else {
                slab = slab_create(cache);
                if (slab) {
                    slab->next = cache->slabs_partial;
                    cache->slabs_partial = slab;
                    obj = slab_alloc_from_slab(cache, slab);
                }
            }
        }
        
        if (!obj) {
            break;
        }
        
        cpu_cache->objects[cpu_cache->available] = obj;
        cpu_cache->available++;
        refilled++;
    }
    
    spinlock_release(&cache->lock);
    
    return refilled > 0 ? 0 : -1;
}

static void slab_cpu_cache_drain(slab_cache_t *cache, int cpu_id) {
    slab_cpu_cache_t *cpu_cache = &cache->cpu_caches[cpu_id];
    
    if (cpu_cache->available == 0 || !cpu_cache->objects) {
        return;
    }
    
    spinlock_acquire(&cache->lock);
    
    uint32_t batch_size = cpu_cache->limit / 2;
    if (batch_size == 0) {
        batch_size = 1;
    }
    
    uint32_t to_drain = cpu_cache->available;
    if (to_drain > batch_size) {
        to_drain = batch_size;
    }
    
    for (uint32_t i = 0; i < to_drain; i++) {
        void *object = cpu_cache->objects[cpu_cache->available - 1];
        cpu_cache->available--;
        
        slab_t *slab = slab_find_for_object(cache, object);
        if (slab) {
            int was_full = (slab->in_use == slab->total_objects);
            slab_free_to_slab(cache, slab, object);
            
            if (was_full) {
                slab_move_to_list(&cache->slabs_full, &cache->slabs_partial, slab);
            } else if (slab->in_use == 0) {
                slab_move_to_list(&cache->slabs_partial, &cache->slabs_free, slab);
            }
        }
    }
    
    spinlock_release(&cache->lock);
}

void slab_get_stats(slab_cache_t *cache, uint64_t *allocs, uint64_t *frees, uint64_t *hits) {
    if (!cache) {
        return;
    }
    
    spinlock_acquire(&cache->lock);
    
    if (allocs) {
        *allocs = cache->total_allocations;
    }
    
    if (frees) {
        *frees = cache->total_frees;
    }
    
    if (hits) {
        *hits = cache->cache_hits;
    }
    
    spinlock_release(&cache->lock);
}
