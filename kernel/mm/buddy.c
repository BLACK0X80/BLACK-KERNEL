#include "../../include/mm/buddy.h"
#include "../../include/mm/gfp.h"
#include "../../include/kernel/config.h"
#include "../../include/kernel/string.h"
#include "../../include/kernel/stdio.h"

static buddy_zone_t g_zones[BUDDY_ZONE_COUNT];
static uint64_t g_memory_start;
static uint64_t g_memory_size;
static uint8_t g_allocation_bitmap[1024 * 1024];

static inline uint64_t pages_to_bytes(uint64_t pages) {
    return pages * BUDDY_PAGE_SIZE;
}

static inline uint64_t bytes_to_pages(uint64_t bytes) {
    return (bytes + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
}

static inline uint64_t addr_to_page_index(uint64_t addr) {
    return (addr - g_memory_start) / BUDDY_PAGE_SIZE;
}

static inline uint64_t page_index_to_addr(uint64_t index) {
    return g_memory_start + (index * BUDDY_PAGE_SIZE);
}

static inline void set_allocation_bit(uint64_t page_index, uint32_t order) {
    uint64_t byte_index = page_index / 8;
    uint8_t bit_index = page_index % 8;
    if (byte_index < sizeof(g_allocation_bitmap)) {
        g_allocation_bitmap[byte_index] |= (1 << bit_index);
    }
}

static inline void clear_allocation_bit(uint64_t page_index, uint32_t order) {
    uint64_t byte_index = page_index / 8;
    uint8_t bit_index = page_index % 8;
    if (byte_index < sizeof(g_allocation_bitmap)) {
        g_allocation_bitmap[byte_index] &= ~(1 << bit_index);
    }
}

static inline int test_allocation_bit(uint64_t page_index) {
    uint64_t byte_index = page_index / 8;
    uint8_t bit_index = page_index % 8;
    if (byte_index < sizeof(g_allocation_bitmap)) {
        return (g_allocation_bitmap[byte_index] >> bit_index) & 1;
    }
    return 0;
}

static inline uint64_t get_buddy_address(uint64_t addr, uint32_t order) {
    uint64_t page_index = addr_to_page_index(addr);
    uint64_t buddy_index = page_index ^ (1ULL << order);
    return page_index_to_addr(buddy_index);
}

static void list_add(buddy_block_t **head, buddy_block_t *block) {
    block->next = *head;
    block->prev = NULL;
    if (*head) {
        (*head)->prev = block;
    }
    *head = block;
}

static void list_remove(buddy_block_t **head, buddy_block_t *block) {
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        *head = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    block->next = NULL;
    block->prev = NULL;
}

void buddy_init(uint64_t memory_start, uint64_t memory_size) {
    g_memory_start = memory_start;
    g_memory_size = memory_size;
    
    for (size_t i = 0; i < sizeof(g_allocation_bitmap); i++) {
        g_allocation_bitmap[i] = 0;
    }
    
    for (int z = 0; z < BUDDY_ZONE_COUNT; z++) {
        buddy_zone_t *zone = &g_zones[z];
        spinlock_init(&zone->lock);
        
        for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
            zone->free_lists[i] = NULL;
            zone->free_counts[i] = 0;
        }
        
        zone->total_pages = 0;
        zone->free_pages = 0;
        zone->base_address = memory_start;
        zone->allocation_map = g_allocation_bitmap;
        zone->map_size = sizeof(g_allocation_bitmap);
    }
    
    buddy_zone_t *zone = &g_zones[BUDDY_ZONE_UNMOVABLE];
    uint64_t total_pages = bytes_to_pages(memory_size);
    zone->total_pages = total_pages;
    zone->free_pages = total_pages;
    
    uint64_t current_addr = memory_start;
    uint64_t remaining_pages = total_pages;
    
    while (remaining_pages > 0) {
        uint32_t order = BUDDY_MAX_ORDER;
        uint64_t order_pages = 1ULL << order;
        
        while (order > 0 && (order_pages > remaining_pages || 
               (addr_to_page_index(current_addr) & (order_pages - 1)) != 0)) {
            order--;
            order_pages = 1ULL << order;
        }
        
        buddy_block_t *block = (buddy_block_t *)(uintptr_t)current_addr;
        list_add(&zone->free_lists[order], block);
        zone->free_counts[order]++;
        
        current_addr += pages_to_bytes(order_pages);
        remaining_pages -= order_pages;
    }
}

uint64_t buddy_alloc_pages(uint32_t order, buddy_zone_type_t zone_type) {
    // Validate order parameter
    if (order > BUDDY_MAX_ORDER) {
        kprintf("[BUDDY] ERROR: Invalid order %u (max %u)\n", order, BUDDY_MAX_ORDER);
        return 0;
    }
    
    // Validate and sanitize zone type
    if (zone_type >= BUDDY_ZONE_COUNT) {
        kprintf("[BUDDY] WARNING: Invalid zone type %u, using UNMOVABLE\n", zone_type);
        zone_type = BUDDY_ZONE_UNMOVABLE;
    }
    
    buddy_zone_t *zone = &g_zones[zone_type];
    
    // Check for NULL zone (shouldn't happen, but defensive)
    if (!zone) {
        kprintf("[BUDDY] ERROR: NULL zone pointer\n");
        return 0;
    }
    
    spinlock_acquire(&zone->lock);
    
    // Find a free block of sufficient size
    uint32_t current_order = order;
    while (current_order <= BUDDY_MAX_ORDER && zone->free_lists[current_order] == NULL) {
        current_order++;
    }
    
    // No free blocks available
    if (current_order > BUDDY_MAX_ORDER) {
        spinlock_release(&zone->lock);
        kprintf("[BUDDY] ERROR: Out of memory (order %u, zone %u)\n", order, zone_type);
        return 0;
    }
    
    buddy_block_t *block = zone->free_lists[current_order];
    list_remove(&zone->free_lists[current_order], block);
    zone->free_counts[current_order]--;
    
    while (current_order > order) {
        current_order--;
        uint64_t block_addr = (uint64_t)(uintptr_t)block;
        uint64_t buddy_addr = block_addr + pages_to_bytes(1ULL << current_order);
        
        buddy_block_t *buddy = (buddy_block_t *)(uintptr_t)buddy_addr;
        list_add(&zone->free_lists[current_order], buddy);
        zone->free_counts[current_order]++;
    }
    
    uint64_t allocated_addr = (uint64_t)(uintptr_t)block;
    uint64_t page_index = addr_to_page_index(allocated_addr);
    set_allocation_bit(page_index, order);
    
    uint64_t allocated_pages = 1ULL << order;
    zone->free_pages -= allocated_pages;
    
    spinlock_release(&zone->lock);
    return allocated_addr;
}

void buddy_free_pages(uint64_t address, uint32_t order) {
    // Validate parameters
    if (order > BUDDY_MAX_ORDER) {
        kprintf("[BUDDY] ERROR: Invalid order %u in free (max %u)\n", order, BUDDY_MAX_ORDER);
        return;
    }
    
    if (address == 0) {
        kprintf("[BUDDY] ERROR: Attempt to free NULL address\n");
        return;
    }
    
    if (address < g_memory_start || address >= g_memory_start + g_memory_size) {
        kprintf("[BUDDY] ERROR: Address 0x%llx out of range [0x%llx, 0x%llx)\n", 
                address, g_memory_start, g_memory_start + g_memory_size);
        return;
    }
    
    // Check alignment
    if (address % BUDDY_PAGE_SIZE != 0) {
        kprintf("[BUDDY] ERROR: Address 0x%llx not page-aligned\n", address);
        return;
    }
    
    buddy_zone_type_t zone_type = BUDDY_ZONE_UNMOVABLE;
    buddy_zone_t *zone = &g_zones[zone_type];
    
    spinlock_acquire(&zone->lock);
    
    uint64_t page_index = addr_to_page_index(address);
    clear_allocation_bit(page_index, order);
    
    uint64_t current_addr = address;
    uint32_t current_order = order;
    
    while (current_order < BUDDY_MAX_ORDER) {
        uint64_t buddy_addr = get_buddy_address(current_addr, current_order);
        
        if (buddy_addr < g_memory_start || buddy_addr >= g_memory_start + g_memory_size) {
            break;
        }
        
        uint64_t buddy_page_index = addr_to_page_index(buddy_addr);
        if (test_allocation_bit(buddy_page_index)) {
            break;
        }
        
        buddy_block_t *buddy_block = NULL;
        buddy_block_t *current = zone->free_lists[current_order];
        while (current) {
            if ((uint64_t)(uintptr_t)current == buddy_addr) {
                buddy_block = current;
                break;
            }
            current = current->next;
        }
        
        if (!buddy_block) {
            break;
        }
        
        list_remove(&zone->free_lists[current_order], buddy_block);
        zone->free_counts[current_order]--;
        
        if (current_addr > buddy_addr) {
            current_addr = buddy_addr;
        }
        
        current_order++;
    }
    
    buddy_block_t *block = (buddy_block_t *)(uintptr_t)current_addr;
    list_add(&zone->free_lists[current_order], block);
    zone->free_counts[current_order]++;
    
    uint64_t freed_pages = 1ULL << order;
    zone->free_pages += freed_pages;
    
    spinlock_release(&zone->lock);
}

uint64_t buddy_get_free_pages(void) {
    uint64_t total_free = 0;
    for (int z = 0; z < BUDDY_ZONE_COUNT; z++) {
        buddy_zone_t *zone = &g_zones[z];
        spinlock_acquire(&zone->lock);
        total_free += zone->free_pages;
        spinlock_release(&zone->lock);
    }
    return total_free;
}

uint64_t buddy_get_total_pages(void) {
    uint64_t total = 0;
    for (int z = 0; z < BUDDY_ZONE_COUNT; z++) {
        buddy_zone_t *zone = &g_zones[z];
        spinlock_acquire(&zone->lock);
        total += zone->total_pages;
        spinlock_release(&zone->lock);
    }
    return total;
}

void buddy_get_order_stats(uint32_t order, uint64_t *free_count) {
    if (order > BUDDY_MAX_ORDER || !free_count) {
        return;
    }
    
    *free_count = 0;
    for (int z = 0; z < BUDDY_ZONE_COUNT; z++) {
        buddy_zone_t *zone = &g_zones[z];
        spinlock_acquire(&zone->lock);
        *free_count += zone->free_counts[order];
        spinlock_release(&zone->lock);
    }
}

void buddy_dump_stats(void) {
    uint64_t total_pages = buddy_get_total_pages();
    uint64_t free_pages = buddy_get_free_pages();
    uint64_t used_pages = total_pages - free_pages;
    
    // Print overall statistics
    uint64_t total_kb = (total_pages * BUDDY_PAGE_SIZE) / 1024;
    uint64_t free_kb = (free_pages * BUDDY_PAGE_SIZE) / 1024;
    uint64_t used_kb = (used_pages * BUDDY_PAGE_SIZE) / 1024;
    
    // Print per-order statistics
    for (uint32_t order = 0; order <= BUDDY_MAX_ORDER; order++) {
        uint64_t free_count = 0;
        buddy_get_order_stats(order, &free_count);
        
        if (free_count > 0) {
            uint64_t order_pages = 1ULL << order;
            uint64_t order_kb = (order_pages * BUDDY_PAGE_SIZE) / 1024;
        }
    }
    
    // Print per-zone statistics
    for (int z = 0; z < BUDDY_ZONE_COUNT; z++) {
        buddy_zone_t *zone = &g_zones[z];
        spinlock_acquire(&zone->lock);
        
        if (zone->total_pages > 0) {
            uint64_t zone_total_kb = (zone->total_pages * BUDDY_PAGE_SIZE) / 1024;
            uint64_t zone_free_kb = (zone->free_pages * BUDDY_PAGE_SIZE) / 1024;
            uint64_t zone_used_kb = zone_total_kb - zone_free_kb;
            
            const char *zone_name = "UNKNOWN";
            if (z == BUDDY_ZONE_UNMOVABLE) zone_name = "UNMOVABLE";
            else if (z == BUDDY_ZONE_RECLAIMABLE) zone_name = "RECLAIMABLE";
            else if (z == BUDDY_ZONE_MOVABLE) zone_name = "MOVABLE";
        }
        
        spinlock_release(&zone->lock);
    }
}

void buddy_dump_zone(buddy_zone_type_t zone_type) {
    if (zone_type >= BUDDY_ZONE_COUNT) {
        return;
    }
    
    buddy_zone_t *zone = &g_zones[zone_type];
    spinlock_acquire(&zone->lock);
    
    const char *zone_name = "UNKNOWN";
    if (zone_type == BUDDY_ZONE_UNMOVABLE) zone_name = "UNMOVABLE";
    else if (zone_type == BUDDY_ZONE_RECLAIMABLE) zone_name = "RECLAIMABLE";
    else if (zone_type == BUDDY_ZONE_MOVABLE) zone_name = "MOVABLE";
    
    uint64_t zone_total_kb = (zone->total_pages * BUDDY_PAGE_SIZE) / 1024;
    uint64_t zone_free_kb = (zone->free_pages * BUDDY_PAGE_SIZE) / 1024;
    uint64_t zone_used_pages = zone->total_pages - zone->free_pages;
    uint64_t zone_used_kb = (zone_used_pages * BUDDY_PAGE_SIZE) / 1024;
    
    // Print per-order free list details
    for (uint32_t order = 0; order <= BUDDY_MAX_ORDER; order++) {
        if (zone->free_counts[order] > 0) {
            uint64_t order_pages = 1ULL << order;
            uint64_t order_kb = (order_pages * BUDDY_PAGE_SIZE) / 1024;
            uint64_t total_free_kb = zone->free_counts[order] * order_kb;
            
            // Count blocks in free list for verification
            uint64_t block_count = 0;
            buddy_block_t *current = zone->free_lists[order];
            while (current && block_count < 100) {  // Limit to prevent infinite loops
                block_count++;
                current = current->next;
            }
        }
    }
    
    spinlock_release(&zone->lock);
}

// Allocation with GFP flags support
uint64_t buddy_alloc_pages_flags(uint32_t order, uint32_t flags) {
    // Validate flags - check for unknown/unsupported flags
    uint32_t valid_flags = GFP_ZONE_MASK | GFP_ZERO | GFP_ATOMIC | GFP_NOWAIT | GFP_DMA | GFP_KERNEL;
    if ((flags & ~valid_flags) != 0) {
        DEBUG_PRINT(BUDDY, "Invalid flags 0x%x detected, proceeding with valid flags only\n", flags);
    }
    
    // Extract zone type from flags with correct priority
    // Priority: MOVABLE > RECLAIMABLE > UNMOVABLE
    buddy_zone_type_t zone_type = BUDDY_ZONE_UNMOVABLE;  // Default
    
    if (flags & GFP_MOVABLE) {
        // MOVABLE has highest priority for user allocations
        zone_type = BUDDY_ZONE_MOVABLE;
        DEBUG_PRINT(BUDDY, "Selected MOVABLE zone for allocation (order %u)\n", order);
    } else if (flags & GFP_RECLAIMABLE) {
        // RECLAIMABLE for kernel caches that can be freed
        zone_type = BUDDY_ZONE_RECLAIMABLE;
        DEBUG_PRINT(BUDDY, "Selected RECLAIMABLE zone for allocation (order %u)\n", order);
    } else {
        // UNMOVABLE for permanent kernel allocations
        DEBUG_PRINT(BUDDY, "Selected UNMOVABLE zone for allocation (order %u)\n", order);
    }
    
    // Allocate pages from selected zone
    uint64_t addr = buddy_alloc_pages(order, zone_type);
    
    if (addr == 0) {
        DEBUG_PRINT(BUDDY, "Allocation failed for order %u from zone %u\n", order, zone_type);
        return 0;
    }
    
    // Handle GFP_ZERO flag - zero-fill allocated pages
    if (flags & GFP_ZERO) {
        uint64_t size = (1ULL << order) * BUDDY_PAGE_SIZE;
        volatile uint8_t *ptr = (volatile uint8_t *)(uintptr_t)addr;
        for (uint64_t i = 0; i < size; i++) {
            ptr[i] = 0;
        }
        DEBUG_PRINT(BUDDY, "Zero-filled %llu bytes at 0x%llx\n", size, addr);
    }
    
    // Handle GFP_NOWAIT - already handled by buddy_alloc_pages returning 0 on failure
    // Handle GFP_ATOMIC - no special handling needed in current implementation
    
    return addr;
}
