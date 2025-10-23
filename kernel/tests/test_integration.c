#include "../../include/mm/buddy.h"
#include "../../include/mm/slab.h"
#include "../../include/mm/pool.h"
#include "../../include/mm/cow.h"
#include "../../include/mm/demand_paging.h"
#include "../../include/mm/page_cache.h"
#include "../../include/mm/gfp.h"
#include "../../include/kernel/heap.h"
#include "../../include/kernel/pmm.h"
#include "../../include/kernel/stdio.h"
#include "../../include/kernel/string.h"

// Test all allocators working together
static int test_all_allocators_together(void) {
    kprintf("[TEST] Testing all allocators together...\n");
    
    // Test buddy allocator
    uint64_t buddy_page = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    if (buddy_page == 0) {
        kprintf("[TEST] FAIL: Buddy allocation failed\n");
        return -1;
    }
    
    // Test slab allocator
    slab_cache_t *cache = slab_cache_create("test-cache", 64, 16);
    if (!cache) {
        kprintf("[TEST] FAIL: Slab cache creation failed\n");
        buddy_free_pages(buddy_page, 0);
        return -1;
    }
    
    void *slab_obj = slab_alloc(cache);
    if (!slab_obj) {
        kprintf("[TEST] FAIL: Slab allocation failed\n");
        slab_cache_destroy(cache);
        buddy_free_pages(buddy_page, 0);
        return -1;
    }
    
    // Test memory pool
    memory_pool_t *pool = pool_create("test-pool", 32, 10);
    if (!pool) {
        kprintf("[TEST] FAIL: Pool creation failed\n");
        slab_free(cache, slab_obj);
        slab_cache_destroy(cache);
        buddy_free_pages(buddy_page, 0);
        return -1;
    }
    
    void *pool_obj = pool_alloc(pool);
    if (!pool_obj) {
        kprintf("[TEST] FAIL: Pool allocation failed\n");
        pool_destroy(pool);
        slab_free(cache, slab_obj);
        slab_cache_destroy(cache);
        buddy_free_pages(buddy_page, 0);
        return -1;
    }
    
    // Test heap allocator (kmalloc)
    void *heap_obj = kmalloc(128);
    if (!heap_obj) {
        kprintf("[TEST] FAIL: Heap allocation failed\n");
        pool_free(pool, pool_obj);
        pool_destroy(pool);
        slab_free(cache, slab_obj);
        slab_cache_destroy(cache);
        buddy_free_pages(buddy_page, 0);
        return -1;
    }
    
    // Clean up in reverse order
    kfree(heap_obj);
    pool_free(pool, pool_obj);
    pool_destroy(pool);
    slab_free(cache, slab_obj);
    slab_cache_destroy(cache);
    buddy_free_pages(buddy_page, 0);
    
    kprintf("[TEST] PASS: All allocators working together\n");
    return 0;
}

// Test memory pressure scenarios
static int test_memory_pressure(void) {
    kprintf("[TEST] Testing memory pressure scenarios...\n");
    
    uint64_t initial_free = buddy_get_free_pages();
    
    // Allocate many pages to create pressure
    #define MAX_ALLOCS 100
    uint64_t allocations[MAX_ALLOCS];
    int alloc_count = 0;
    
    for (int i = 0; i < MAX_ALLOCS; i++) {
        uint64_t page = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
        if (page == 0) {
            kprintf("[TEST] Memory exhausted after %d allocations\n", i);
            break;
        }
        allocations[alloc_count++] = page;
    }
    
    if (alloc_count == 0) {
        kprintf("[TEST] FAIL: Could not allocate any pages\n");
        return -1;
    }
    
    // Free half of the allocations
    for (int i = 0; i < alloc_count / 2; i++) {
        buddy_free_pages(allocations[i], 0);
    }
    
    // Try to allocate again
    int new_allocs = 0;
    for (int i = alloc_count / 2; i < alloc_count; i++) {
        uint64_t page = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
        if (page != 0) {
            allocations[i] = page;
            new_allocs++;
        }
    }
    
    // Free all remaining allocations
    for (int i = alloc_count / 2; i < alloc_count; i++) {
        if (allocations[i] != 0) {
            buddy_free_pages(allocations[i], 0);
        }
    }
    
    uint64_t final_free = buddy_get_free_pages();
    
    kprintf("[TEST] Initial free: %llu, Final free: %llu\n", initial_free, final_free);
    kprintf("[TEST] PASS: Memory pressure handling\n");
    return 0;
}

// Test error handling paths
static int test_error_handling(void) {
    kprintf("[TEST] Testing error handling...\n");
    
    // Test NULL pointer handling
    kfree(NULL);
    slab_free(NULL, NULL);
    pool_free(NULL, NULL);
    
    // Test invalid parameters
    uint64_t invalid_page = buddy_alloc_pages(99, BUDDY_ZONE_UNMOVABLE);
    if (invalid_page != 0) {
        kprintf("[TEST] FAIL: Invalid order should return 0\n");
        return -1;
    }
    
    // Test freeing invalid address
    buddy_free_pages(0, 0);
    buddy_free_pages(0xDEADBEEF, 0);
    
    // Test slab with NULL name
    slab_cache_t *null_cache = slab_cache_create(NULL, 64, 16);
    if (null_cache != NULL) {
        kprintf("[TEST] FAIL: NULL name should fail\n");
        return -1;
    }
    
    // Test slab with zero size
    slab_cache_t *zero_cache = slab_cache_create("zero", 0, 16);
    if (zero_cache != NULL) {
        kprintf("[TEST] FAIL: Zero size should fail\n");
        return -1;
    }
    
    kprintf("[TEST] PASS: Error handling\n");
    return 0;
}

// Test for memory leaks
static int test_no_memory_leaks(void) {
    kprintf("[TEST] Testing for memory leaks...\n");
    
    uint64_t initial_free = buddy_get_free_pages();
    
    // Perform many allocations and frees
    for (int iteration = 0; iteration < 10; iteration++) {
        // Buddy allocations
        uint64_t pages[10];
        for (int i = 0; i < 10; i++) {
            pages[i] = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
        }
        for (int i = 0; i < 10; i++) {
            if (pages[i] != 0) {
                buddy_free_pages(pages[i], 0);
            }
        }
        
        // Slab allocations
        slab_cache_t *cache = slab_cache_create("leak-test", 64, 16);
        if (cache) {
            void *objs[10];
            for (int i = 0; i < 10; i++) {
                objs[i] = slab_alloc(cache);
            }
            for (int i = 0; i < 10; i++) {
                if (objs[i]) {
                    slab_free(cache, objs[i]);
                }
            }
            slab_cache_destroy(cache);
        }
        
        // Heap allocations
        void *heap_objs[10];
        for (int i = 0; i < 10; i++) {
            heap_objs[i] = kmalloc(64);
        }
        for (int i = 0; i < 10; i++) {
            kfree(heap_objs[i]);
        }
    }
    
    uint64_t final_free = buddy_get_free_pages();
    
    // Allow some tolerance for internal structures
    int64_t diff = (int64_t)final_free - (int64_t)initial_free;
    if (diff < -10) {
        kprintf("[TEST] FAIL: Memory leak detected (lost %lld pages)\n", -diff);
        return -1;
    }
    
    kprintf("[TEST] PASS: No significant memory leaks\n");
    return 0;
}

// Test GFP flags
static int test_gfp_flags(void) {
    kprintf("[TEST] Testing GFP flags...\n");
    
    // Test GFP_ZERO flag
    uint64_t zero_page = buddy_alloc_pages_flags(0, GFP_ZERO);
    if (zero_page == 0) {
        kprintf("[TEST] FAIL: GFP_ZERO allocation failed\n");
        return -1;
    }
    
    // Verify page is zeroed
    uint8_t *ptr = (uint8_t *)(uintptr_t)zero_page;
    int all_zero = 1;
    for (int i = 0; i < 4096; i++) {
        if (ptr[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    
    if (!all_zero) {
        kprintf("[TEST] FAIL: GFP_ZERO did not zero memory\n");
        buddy_free_pages(zero_page, 0);
        return -1;
    }
    
    buddy_free_pages(zero_page, 0);
    
    // Test different zone types
    uint64_t unmovable = buddy_alloc_pages_flags(0, GFP_UNMOVABLE);
    uint64_t reclaimable = buddy_alloc_pages_flags(0, GFP_RECLAIMABLE);
    uint64_t movable = buddy_alloc_pages_flags(0, GFP_MOVABLE);
    
    if (unmovable) buddy_free_pages(unmovable, 0);
    if (reclaimable) buddy_free_pages(reclaimable, 0);
    if (movable) buddy_free_pages(movable, 0);
    
    // Test kmalloc with flags
    void *zero_obj = kmalloc_flags(128, GFP_ZERO);
    if (!zero_obj) {
        kprintf("[TEST] FAIL: kmalloc_flags failed\n");
        return -1;
    }
    
    kfree(zero_obj);
    
    kprintf("[TEST] PASS: GFP flags\n");
    return 0;
}

// Main integration test runner
int run_integration_tests(void) {
    kprintf("\n=== Running Integration Tests ===\n");
    
    int failures = 0;
    
    if (test_all_allocators_together() != 0) failures++;
    if (test_memory_pressure() != 0) failures++;
    if (test_error_handling() != 0) failures++;
    if (test_no_memory_leaks() != 0) failures++;
    if (test_gfp_flags() != 0) failures++;
    
    kprintf("\n=== Integration Tests Complete ===\n");
    kprintf("Failures: %d\n", failures);
    
    return failures;
}

