#pragma once
#include "../kernel/types.h"
#include "../kernel/spinlock.h"

#define POOL_NAME_MAX 32

typedef struct pool_chunk {
    struct pool_chunk *next;
} pool_chunk_t;

typedef struct pool_region {
    void *base;
    struct pool_region *next;
} pool_region_t;

typedef struct memory_pool {
    char name[POOL_NAME_MAX];
    size_t object_size;
    uint32_t initial_count;
    uint32_t grow_count;
    uint32_t total_objects;
    uint32_t free_objects;
    pool_chunk_t *free_list;
    pool_region_t *regions;
    spinlock_t lock;
} memory_pool_t;

memory_pool_t *pool_create(const char *name, size_t object_size, uint32_t initial_count);
void pool_destroy(memory_pool_t *pool);
void *pool_alloc(memory_pool_t *pool);
void pool_free(memory_pool_t *pool, void *object);
uint32_t pool_get_utilization(memory_pool_t *pool);
