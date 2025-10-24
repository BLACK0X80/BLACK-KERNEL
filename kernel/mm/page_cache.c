#include "../../include/mm/page_cache.h"
#include "../../include/mm/buddy.h"
#include "../../include/kernel/config.h"
#include "../../include/kernel/string.h"
#include "../../include/kernel/stdio.h"

// Hash size must be power of 2 for efficient bitwise AND hashing
#define DEFAULT_HASH_SIZE 1024
#define HASH_SIZE_MASK (DEFAULT_HASH_SIZE - 1)

static page_cache_t g_page_cache;

static inline uint32_t hash_function(uint64_t file_id, uint64_t offset) {
    // Mix file_id and offset using XOR for better distribution
    uint64_t hash = file_id ^ (offset >> 12);  // Offset in pages
    
    // Multiply by golden ratio prime for avalanche effect
    hash = hash * 2654435761ULL;
    
    // Use bitwise AND instead of modulo (20-40x faster)
    // This works because hash_size is guaranteed to be power of 2
    return (uint32_t)(hash & HASH_SIZE_MASK);
}

void page_cache_init(uint64_t max_pages) {
    spinlock_init(&g_page_cache.lock);
    
    g_page_cache.hash_size = DEFAULT_HASH_SIZE;
    
    // Validate that hash_size is power of 2
    if ((g_page_cache.hash_size & (g_page_cache.hash_size - 1)) != 0) {
        kprintf("[PAGE_CACHE] ERROR: hash_size %llu is not power of 2, using 1024\n",
                g_page_cache.hash_size);
        g_page_cache.hash_size = 1024;
    }
    
    DEBUG_PRINT(PAGE_CACHE, "Initialized with hash_size=%llu, max_pages=%llu\n",
                g_page_cache.hash_size, max_pages);
    
    g_page_cache.max_pages = max_pages;
    g_page_cache.total_pages = 0;
    g_page_cache.cache_hits = 0;
    g_page_cache.cache_misses = 0;
    g_page_cache.lru_head = NULL;
    g_page_cache.lru_tail = NULL;
    
    uint64_t hash_table_pages = (g_page_cache.hash_size * sizeof(page_cache_entry_t *) + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
    uint64_t hash_table_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    
    if (hash_table_addr == 0) {
        g_page_cache.hash_table = NULL;
        return;
    }
    
    g_page_cache.hash_table = (page_cache_entry_t **)(uintptr_t)hash_table_addr;
    memset(g_page_cache.hash_table, 0, g_page_cache.hash_size * sizeof(page_cache_entry_t *));
}

static uint64_t get_timestamp(void) {
    // Simple timestamp counter - in real implementation would use TSC or timer
    static uint64_t counter = 0;
    return counter++;
}

uint64_t page_cache_lookup(uint64_t file_id, uint64_t offset) {
    if (g_page_cache.hash_table == NULL) {
        return 0;
    }
    
    spinlock_acquire(&g_page_cache.lock);
    
    uint32_t hash = hash_function(file_id, offset);
    page_cache_entry_t *entry = g_page_cache.hash_table[hash];
    
    while (entry != NULL) {
        if (entry->file_id == file_id && entry->offset == offset) {
            // Cache hit - update access timestamp
            entry->last_access = get_timestamp();
            g_page_cache.cache_hits++;
            
            uint64_t phys_addr = entry->physical_address;
            spinlock_release(&g_page_cache.lock);
            return phys_addr;
        }
        entry = entry->hash_next;
    }
    
    // Cache miss
    g_page_cache.cache_misses++;
    spinlock_release(&g_page_cache.lock);
    return 0;
}

static void lru_add_to_head(page_cache_entry_t *entry) {
    entry->lru_next = g_page_cache.lru_head;
    entry->lru_prev = NULL;
    
    if (g_page_cache.lru_head != NULL) {
        g_page_cache.lru_head->lru_prev = entry;
    }
    
    g_page_cache.lru_head = entry;
    
    if (g_page_cache.lru_tail == NULL) {
        g_page_cache.lru_tail = entry;
    }
}

static void lru_remove(page_cache_entry_t *entry) {
    if (entry->lru_prev != NULL) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        g_page_cache.lru_head = entry->lru_next;
    }
    
    if (entry->lru_next != NULL) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        g_page_cache.lru_tail = entry->lru_prev;
    }
    
    entry->lru_next = NULL;
    entry->lru_prev = NULL;
}

int page_cache_insert(uint64_t file_id, uint64_t offset, uint64_t phys_addr) {
    if (g_page_cache.hash_table == NULL || phys_addr == 0) {
        return -1;
    }
    
    spinlock_acquire(&g_page_cache.lock);
    
    // Check if already exists
    uint32_t hash = hash_function(file_id, offset);
    page_cache_entry_t *existing = g_page_cache.hash_table[hash];
    
    while (existing != NULL) {
        if (existing->file_id == file_id && existing->offset == offset) {
            // Already cached
            spinlock_release(&g_page_cache.lock);
            return 0;
        }
        existing = existing->hash_next;
    }
    
    // Evict LRU page if cache is full
    if (g_page_cache.total_pages >= g_page_cache.max_pages) {
        spinlock_release(&g_page_cache.lock);
        page_cache_evict_lru();
        spinlock_acquire(&g_page_cache.lock);
    }
    
    // Allocate new entry
    uint64_t entry_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    if (entry_addr == 0) {
        spinlock_release(&g_page_cache.lock);
        return -1;
    }
    
    page_cache_entry_t *entry = (page_cache_entry_t *)(uintptr_t)entry_addr;
    entry->file_id = file_id;
    entry->offset = offset;
    entry->physical_address = phys_addr;
    entry->last_access = get_timestamp();
    entry->flags = 0;
    entry->hash_next = NULL;
    entry->lru_next = NULL;
    entry->lru_prev = NULL;
    
    // Add to hash table
    entry->hash_next = g_page_cache.hash_table[hash];
    g_page_cache.hash_table[hash] = entry;
    
    // Add to LRU list
    lru_add_to_head(entry);
    
    g_page_cache.total_pages++;
    
    spinlock_release(&g_page_cache.lock);
    return 0;
}

void page_cache_evict_lru(void) {
    if (g_page_cache.hash_table == NULL) {
        return;
    }
    
    spinlock_acquire(&g_page_cache.lock);
    
    if (g_page_cache.lru_tail == NULL) {
        spinlock_release(&g_page_cache.lock);
        return;
    }
    
    page_cache_entry_t *victim = g_page_cache.lru_tail;
    
    // Remove from LRU list
    lru_remove(victim);
    
    // Remove from hash table
    uint32_t hash = hash_function(victim->file_id, victim->offset);
    page_cache_entry_t *entry = g_page_cache.hash_table[hash];
    page_cache_entry_t *prev = NULL;
    
    while (entry != NULL) {
        if (entry == victim) {
            if (prev == NULL) {
                g_page_cache.hash_table[hash] = entry->hash_next;
            } else {
                prev->hash_next = entry->hash_next;
            }
            break;
        }
        prev = entry;
        entry = entry->hash_next;
    }
    
    g_page_cache.total_pages--;
    
    // Free physical page
    buddy_free_pages(victim->physical_address, 0);
    
    // Free entry structure
    buddy_free_pages((uint64_t)(uintptr_t)victim, 0);
    
    spinlock_release(&g_page_cache.lock);
}

void page_cache_remove(uint64_t file_id, uint64_t offset) {
    if (g_page_cache.hash_table == NULL) {
        return;
    }
    
    spinlock_acquire(&g_page_cache.lock);
    
    uint32_t hash = hash_function(file_id, offset);
    page_cache_entry_t *entry = g_page_cache.hash_table[hash];
    page_cache_entry_t *prev = NULL;
    
    while (entry != NULL) {
        if (entry->file_id == file_id && entry->offset == offset) {
            // Remove from hash table
            if (prev == NULL) {
                g_page_cache.hash_table[hash] = entry->hash_next;
            } else {
                prev->hash_next = entry->hash_next;
            }
            
            // Remove from LRU list
            lru_remove(entry);
            
            g_page_cache.total_pages--;
            
            // Free entry structure (but not the physical page - caller manages that)
            buddy_free_pages((uint64_t)(uintptr_t)entry, 0);
            
            spinlock_release(&g_page_cache.lock);
            return;
        }
        prev = entry;
        entry = entry->hash_next;
    }
    
    spinlock_release(&g_page_cache.lock);
}

void page_cache_get_stats(uint64_t *hits, uint64_t *misses, uint64_t *pages) {
    if (g_page_cache.hash_table == NULL) {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        if (pages) *pages = 0;
        return;
    }
    
    spinlock_acquire(&g_page_cache.lock);
    
    if (hits) {
        *hits = g_page_cache.cache_hits;
    }
    
    if (misses) {
        *misses = g_page_cache.cache_misses;
    }
    
    if (pages) {
        *pages = g_page_cache.total_pages;
    }
    
    spinlock_release(&g_page_cache.lock);
}
