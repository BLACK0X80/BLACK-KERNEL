#include "../../include/mm/page_cache.h"
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

void test_page_cache_init(void) {
    page_cache_init(100);
    
    uint64_t hits, misses, pages;
    page_cache_get_stats(&hits, &misses, &pages);
    
    TEST_ASSERT(hits == 0, "Initial cache hits should be 0");
    TEST_ASSERT(misses == 0, "Initial cache misses should be 0");
    TEST_ASSERT(pages == 0, "Initial cached pages should be 0");
}

void test_page_cache_insert_and_lookup(void) {
    page_cache_init(100);
    
    uint64_t phys_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(phys_addr != 0, "Physical page allocation should succeed");
    
    int result = page_cache_insert(1, 0, phys_addr);
    TEST_ASSERT(result == 0, "Page cache insertion should succeed");
    
    uint64_t found_addr = page_cache_lookup(1, 0);
    TEST_ASSERT(found_addr == phys_addr, "Lookup should return inserted physical address");
    
    uint64_t hits, misses, pages;
    page_cache_get_stats(&hits, &misses, &pages);
    TEST_ASSERT(hits == 1, "Cache hits should be 1 after successful lookup");
    TEST_ASSERT(pages == 1, "Cached pages should be 1 after insertion");
}

void test_page_cache_miss(void) {
    page_cache_init(100);
    
    uint64_t found_addr = page_cache_lookup(999, 999);
    TEST_ASSERT(found_addr == 0, "Lookup of non-existent page should return 0");
    
    uint64_t hits, misses, pages;
    page_cache_get_stats(&hits, &misses, &pages);
    TEST_ASSERT(misses == 1, "Cache misses should be 1 after failed lookup");
}

void test_page_cache_hash_collisions(void) {
    page_cache_init(100);
    
    uint64_t phys_addr1 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t phys_addr2 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t phys_addr3 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    
    TEST_ASSERT(phys_addr1 != 0 && phys_addr2 != 0 && phys_addr3 != 0, 
                "All physical page allocations should succeed");
    
    page_cache_insert(1, 0, phys_addr1);
    page_cache_insert(2, 0, phys_addr2);
    page_cache_insert(3, 0, phys_addr3);
    
    uint64_t found1 = page_cache_lookup(1, 0);
    uint64_t found2 = page_cache_lookup(2, 0);
    uint64_t found3 = page_cache_lookup(3, 0);
    
    TEST_ASSERT(found1 == phys_addr1, "First entry should be retrievable");
    TEST_ASSERT(found2 == phys_addr2, "Second entry should be retrievable");
    TEST_ASSERT(found3 == phys_addr3, "Third entry should be retrievable");
}

void test_page_cache_lru_eviction(void) {
    page_cache_init(3);
    
    uint64_t phys_addr1 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t phys_addr2 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t phys_addr3 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t phys_addr4 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    
    page_cache_insert(1, 0, phys_addr1);
    page_cache_insert(2, 0, phys_addr2);
    page_cache_insert(3, 0, phys_addr3);
    
    uint64_t hits, misses, pages;
    page_cache_get_stats(&hits, &misses, &pages);
    TEST_ASSERT(pages == 3, "Cache should have 3 pages before eviction");
    
    page_cache_insert(4, 0, phys_addr4);
    
    page_cache_get_stats(&hits, &misses, &pages);
    TEST_ASSERT(pages == 3, "Cache should still have 3 pages after eviction");
    
    uint64_t found1 = page_cache_lookup(1, 0);
    TEST_ASSERT(found1 == 0, "First (LRU) entry should have been evicted");
    
    uint64_t found4 = page_cache_lookup(4, 0);
    TEST_ASSERT(found4 == phys_addr4, "Newly inserted entry should be present");
}

void test_page_cache_removal(void) {
    page_cache_init(100);
    
    uint64_t phys_addr = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    page_cache_insert(1, 0, phys_addr);
    
    uint64_t found_before = page_cache_lookup(1, 0);
    TEST_ASSERT(found_before == phys_addr, "Entry should exist before removal");
    
    page_cache_remove(1, 0);
    
    uint64_t found_after = page_cache_lookup(1, 0);
    TEST_ASSERT(found_after == 0, "Entry should not exist after removal");
    
    uint64_t hits, misses, pages;
    page_cache_get_stats(&hits, &misses, &pages);
    TEST_ASSERT(pages == 0, "Cached pages should be 0 after removal");
    
    buddy_free_pages(phys_addr, 0);
}

void test_page_cache_hit_rate(void) {
    page_cache_init(100);
    
    uint64_t phys_addr1 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    uint64_t phys_addr2 = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    
    page_cache_insert(1, 0, phys_addr1);
    page_cache_insert(2, 0, phys_addr2);
    
    page_cache_lookup(1, 0);
    page_cache_lookup(1, 0);
    page_cache_lookup(2, 0);
    page_cache_lookup(3, 0);
    
    uint64_t hits, misses, pages;
    page_cache_get_stats(&hits, &misses, &pages);
    
    TEST_ASSERT(hits == 3, "Should have 3 cache hits");
    TEST_ASSERT(misses == 1, "Should have 1 cache miss");
    
    uint64_t total = hits + misses;
    uint64_t hit_rate = (hits * 100) / total;
    TEST_ASSERT(hit_rate == 75, "Hit rate should be 75%");
}

void run_page_cache_tests(void) {
    kprintf("Running page cache unit tests...\n");
    
    test_page_cache_init();
    test_page_cache_insert_and_lookup();
    test_page_cache_miss();
    test_page_cache_hash_collisions();
    test_page_cache_lru_eviction();
    test_page_cache_removal();
    test_page_cache_hit_rate();
    
    kprintf("\nPage Cache Tests: %d/%d passed\n", test_passed, test_count);
    
    
}

