#pragma once
#include "../kernel/types.h"
#include "../kernel/spinlock.h"

#define SLAB_CACHE_NAME_MAX 32
#define SLAB_CPU_CACHE_SIZE 16

typedef struct slab {
    struct slab *next;
    void *free_list;
    uint32_t in_use;
    uint32_t total_objects;
    uint8_t *objects;
} slab_t;

typedef struct slab_cpu_cache {
    void **objects;
    uint32_t available;
    uint32_t limit;
} slab_cpu_cache_t;

typedef struct slab_cache {
    char name[SLAB_CACHE_NAME_MAX];
    size_t object_size;
    size_t align;
    uint32_t objects_per_slab;
    uint32_t color_next;
    slab_t *slabs_full;
    slab_t *slabs_partial;
    slab_t *slabs_free;
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t cache_hits;
    spinlock_t lock;
    slab_cpu_cache_t cpu_caches[8];
    struct slab_cache *next;
} slab_cache_t;

void slab_init(void);
slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align);
void slab_cache_destroy(slab_cache_t *cache);
void *slab_alloc(slab_cache_t *cache);
void slab_free(slab_cache_t *cache, void *object);
void slab_get_stats(slab_cache_t *cache, uint64_t *allocs, uint64_t *frees, uint64_t *hits);
