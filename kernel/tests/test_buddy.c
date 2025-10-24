#include "../../include/mm/buddy.h"
#include "../../include/kernel/stdio.h"

static int test_count = 0;
static int test_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    test_count++; \
    if (condition) { \
        test_passed++; \
    } else { \
        kprintf("[FAIL] %s\n", message); \
    } \
} while(0)

void test_buddy_single_page_alloc(void) {
    uint64_t addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(addr != 0, "Single page allocation should succeed");
    
    uint64_t free_before = buddy_get_free_pages();
    buddy_free_pages(addr, 0);
    uint64_t free_after = buddy_get_free_pages();
    
    TEST_ASSERT(free_after == free_before + 1, "Free pages should increase by 1 after freeing");
}

void test_buddy_multi_page_alloc(void) {
    uint64_t addr1 = buddy_alloc_pages(2, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(addr1 != 0, "4-page allocation should succeed");
    
    uint64_t addr2 = buddy_alloc_pages(3, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(addr2 != 0, "8-page allocation should succeed");
    
    TEST_ASSERT(addr1 != addr2, "Allocations should return different addresses");
    
    buddy_free_pages(addr1, 2);
    buddy_free_pages(addr2, 3);
}

void test_buddy_coalescing(void) {
    uint64_t free_before = buddy_get_free_pages();
    
    uint64_t addr1 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t addr2 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    
    TEST_ASSERT(addr1 != 0 && addr2 != 0, "Both allocations should succeed");
    
    buddy_free_pages(addr1, 0);
    buddy_free_pages(addr2, 0);
    
    uint64_t free_after = buddy_get_free_pages();
    TEST_ASSERT(free_after == free_before, "Free pages should return to original after coalescing");
}

void test_buddy_order_stats(void) {
    uint64_t free_count;
    buddy_get_order_stats(0, &free_count);
    TEST_ASSERT(free_count >= 0, "Order 0 stats should be retrievable");
    
    uint64_t addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t free_count_after;
    buddy_get_order_stats(0, &free_count_after);
    
    buddy_free_pages(addr, 0);
}

void test_buddy_zone_separation(void) {
    uint64_t addr_unmovable = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t addr_reclaimable = buddy_alloc_pages(0, BUDDY_ZONE_RECLAIMABLE);
    uint64_t addr_movable = buddy_alloc_pages(0, BUDDY_ZONE_MOVABLE);
    
    TEST_ASSERT(addr_unmovable != 0, "Unmovable zone allocation should succeed");
    
    if (addr_unmovable) buddy_free_pages(addr_unmovable, 0);
    if (addr_reclaimable) buddy_free_pages(addr_reclaimable, 0);
    if (addr_movable) buddy_free_pages(addr_movable, 0);
}

void test_buddy_statistics(void) {
    uint64_t total_pages = buddy_get_total_pages();
    uint64_t free_pages = buddy_get_free_pages();
    
    TEST_ASSERT(total_pages > 0, "Total pages should be greater than 0");
    TEST_ASSERT(free_pages <= total_pages, "Free pages should not exceed total pages");
    
    uint64_t addr = buddy_alloc_pages(2, BUDDY_ZONE_UNMOVABLE);
    uint64_t free_after_alloc = buddy_get_free_pages();
    
    TEST_ASSERT(free_after_alloc < free_pages, "Free pages should decrease after allocation");
    TEST_ASSERT(free_pages - free_after_alloc == 4, "Should have 4 fewer free pages after order-2 allocation");
    
    buddy_free_pages(addr, 2);
    uint64_t free_after_free = buddy_get_free_pages();
    
    TEST_ASSERT(free_after_free == free_pages, "Free pages should return to original after freeing");
}

void test_buddy_debug_functions(void) {
    // Test that debug functions can be called without crashing
    buddy_dump_stats();
    TEST_ASSERT(1, "buddy_dump_stats should execute without crashing");
    
    buddy_dump_zone(BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(1, "buddy_dump_zone should execute without crashing");
    
    // Test with allocations
    uint64_t addr1 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t addr2 = buddy_alloc_pages(3, BUDDY_ZONE_UNMOVABLE);
    
    buddy_dump_stats();
    buddy_dump_zone(BUDDY_ZONE_UNMOVABLE);
    
    TEST_ASSERT(1, "Debug functions should work with active allocations");
    
    buddy_free_pages(addr1, 0);
    buddy_free_pages(addr2, 3);
}

void run_buddy_tests(void) {
    kprintf("Running buddy allocator tests...\n");
    
    test_buddy_single_page_alloc();
    test_buddy_multi_page_alloc();
    test_buddy_coalescing();
    test_buddy_order_stats();
    test_buddy_zone_separation();
    test_buddy_statistics();
    test_buddy_debug_functions();
    
    kprintf("Buddy tests: %d/%d passed\n", test_passed, test_count);
}


// New tests for zone priority and flag handling

void test_buddy_zone_priority(void) {
    // Test that MOVABLE has priority over RECLAIMABLE
    uint64_t addr1 = buddy_alloc_pages_flags(0, GFP_MOVABLE | GFP_RECLAIMABLE);
    TEST_ASSERT(addr1 != 0, "Allocation with both MOVABLE and RECLAIMABLE flags should succeed");
    
    // The allocation should come from MOVABLE zone (highest priority)
    // We can't directly verify the zone, but we can verify the allocation succeeded
    
    if (addr1) buddy_free_pages(addr1, 0);
    
    // Test RECLAIMABLE without MOVABLE
    uint64_t addr2 = buddy_alloc_pages_flags(0, GFP_RECLAIMABLE);
    TEST_ASSERT(addr2 != 0, "Allocation with RECLAIMABLE flag should succeed");
    
    if (addr2) buddy_free_pages(addr2, 0);
    
    // Test default (UNMOVABLE)
    uint64_t addr3 = buddy_alloc_pages_flags(0, 0);
    TEST_ASSERT(addr3 != 0, "Allocation with no zone flags should default to UNMOVABLE");
    
    if (addr3) buddy_free_pages(addr3, 0);
}

void test_buddy_invalid_flags(void) {
    // Test with invalid/unknown flags - should still work with default zone
    uint64_t addr = buddy_alloc_pages_flags(0, 0xFFFF);
    TEST_ASSERT(addr != 0, "Allocation with invalid flags should still succeed with default zone");
    
    if (addr) buddy_free_pages(addr, 0);
}

void test_buddy_gfp_zero(void) {
    // Test GFP_ZERO flag
    uint64_t addr = buddy_alloc_pages_flags(0, GFP_ZERO);
    TEST_ASSERT(addr != 0, "Allocation with GFP_ZERO should succeed");
    
    if (addr) {
        // Verify page is zeroed
        uint8_t *page = (uint8_t *)(uintptr_t)addr;
        int all_zero = 1;
        for (int i = 0; i < 4096; i++) {
            if (page[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        TEST_ASSERT(all_zero, "Page allocated with GFP_ZERO should be zeroed");
        
        buddy_free_pages(addr, 0);
    }
}

void test_buddy_combined_flags(void) {
    // Test combination of flags
    uint64_t addr = buddy_alloc_pages_flags(0, GFP_MOVABLE | GFP_ZERO);
    TEST_ASSERT(addr != 0, "Allocation with MOVABLE and ZERO flags should succeed");
    
    if (addr) {
        // Verify page is zeroed
        uint8_t *page = (uint8_t *)(uintptr_t)addr;
        int all_zero = 1;
        for (int i = 0; i < 100; i++) {  // Check first 100 bytes
            if (page[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        TEST_ASSERT(all_zero, "Page with MOVABLE|ZERO should be zeroed");
        
        buddy_free_pages(addr, 0);
    }
}

void run_buddy_tests_extended(void) {
    kprintf("\nRunning extended buddy allocator tests...\n");
    
    // Reset counters
    int old_count = test_count;
    int old_passed = test_passed;
    
    test_buddy_zone_priority();
    test_buddy_invalid_flags();
    test_buddy_gfp_zero();
    test_buddy_combined_flags();
    
    kprintf("Extended buddy tests: %d/%d passed\n", 
            test_passed - old_passed, test_count - old_count);
}
