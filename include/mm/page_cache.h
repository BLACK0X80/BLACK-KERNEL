#pragma once
#include "../kernel/types.h"
#include "../kernel/spinlock.h"

typedef struct page_cache_entry {
    uint64_t file_id;
    uint64_t offset;
    uint64_t physical_address;
    uint64_t last_access;
    uint32_t flags;
    struct page_cache_entry *hash_next;
    struct page_cache_entry *lru_next;
    struct page_cache_entry *lru_prev;
} page_cache_entry_t;

typedef struct page_cache {
    page_cache_entry_t **hash_table;
    uint32_t hash_size;
    page_cache_entry_t *lru_head;
    page_cache_entry_t *lru_tail;
    uint64_t total_pages;
    uint64_t max_pages;
    uint64_t cache_hits;
    uint64_t cache_misses;
    spinlock_t lock;
} page_cache_t;

void page_cache_init(uint64_t max_pages);
uint64_t page_cache_lookup(uint64_t file_id, uint64_t offset);
int page_cache_insert(uint64_t file_id, uint64_t offset, uint64_t phys_addr);
void page_cache_remove(uint64_t file_id, uint64_t offset);
void page_cache_evict_lru(void);
void page_cache_get_stats(uint64_t *hits, uint64_t *misses, uint64_t *pages);
