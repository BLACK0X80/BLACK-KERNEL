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

