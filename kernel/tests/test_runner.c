#include "../../include/kernel/stdio.h"
#include "../../include/mm/buddy.h"
#include "../../include/mm/slab.h"
#include "../../include/mm/pool.h"
#include "../../include/mm/cow.h"
#include "../../include/mm/demand_paging.h"
#include "../../include/mm/page_cache.h"

// Forward declarations of test functions
extern void run_buddy_tests(void);
extern void run_slab_tests(void);
extern void run_pool_tests(void);
extern void run_cow_tests(void);
extern void run_demand_paging_tests(void);
extern void run_page_cache_tests(void);
extern void run_integration_tests(void);

void run_all_memory_tests(void) {
    kprintf("\n");
    kprintf("========================================\n");
    kprintf("  MEMORY MANAGEMENT TEST SUITE\n");
    kprintf("========================================\n\n");
    
    kprintf("[TEST SUITE] Running Buddy Allocator Tests...\n");
    run_buddy_tests();
    
    kprintf("\n[TEST SUITE] Running Slab Allocator Tests...\n");
    run_slab_tests();
    
    kprintf("\n[TEST SUITE] Running Memory Pool Tests...\n");
    run_pool_tests();
    
    kprintf("\n[TEST SUITE] Running COW Tests...\n");
    run_cow_tests();
    
    kprintf("\n[TEST SUITE] Running Demand Paging Tests...\n");
    run_demand_paging_tests();
    
    kprintf("\n[TEST SUITE] Running Page Cache Tests...\n");
    run_page_cache_tests();
    
    kprintf("\n[TEST SUITE] Running Integration Tests...\n");
    run_integration_tests();
    
    kprintf("\n");
    kprintf("========================================\n");
    kprintf("  ALL TESTS COMPLETED\n");
    kprintf("========================================\n\n");
}
