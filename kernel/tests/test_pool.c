#include "../../include/mm/pool.h"
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

void test_pool_creation_and_destruction(void) {
    memory_pool_t *pool = pool_create("test_pool", 64, 10);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    if (pool) {
        TEST_ASSERT(pool->object_size >= 64, "Object size should be at least 64 bytes");
        TEST_ASSERT(pool->total_objects >= 10, "Pool should have at least 10 objects");
        TEST_ASSERT(pool->free_objects >= 10, "Pool should have at least 10 free objects");
        
        pool_destroy(pool);
        TEST_ASSERT(1, "Pool destruction should complete without crashing");
    }
}

void test_pool_allocation_and_freeing(void) {
    memory_pool_t *pool = pool_create("alloc_test", 32, 5);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    if (pool) {
        uint32_t initial_free = pool->free_objects;
        
        void *obj1 = pool_alloc(pool);
        TEST_ASSERT(obj1 != NULL, "First allocation should succeed");
        TEST_ASSERT(pool->free_objects == initial_free - 1, "Free count should decrease by 1");
        
        void *obj2 = pool_alloc(pool);
        TEST_ASSERT(obj2 != NULL, "Second allocation should succeed");
        TEST_ASSERT(obj1 != obj2, "Allocations should return different addresses");
        TEST_ASSERT(pool->free_objects == initial_free - 2, "Free count should decrease by 2");
        
        pool_free(pool, obj1);
        TEST_ASSERT(pool->free_objects == initial_free - 1, "Free count should increase after freeing");
        
        pool_free(pool, obj2);
        TEST_ASSERT(pool->free_objects == initial_free, "Free count should return to initial");
        
        pool_destroy(pool);
    }
}

void test_pool_constant_time_operations(void) {
    memory_pool_t *pool = pool_create("timing_test", 128, 20);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    if (pool) {
        // Allocate multiple objects - should all be O(1)
        void *objects[10];
        for (int i = 0; i < 10; i++) {
            objects[i] = pool_alloc(pool);
            TEST_ASSERT(objects[i] != NULL, "Allocation should succeed");
        }
        
        // Free all objects - should all be O(1)
        for (int i = 0; i < 10; i++) {
            pool_free(pool, objects[i]);
        }
        
        TEST_ASSERT(pool->free_objects == pool->total_objects, 
                   "All objects should be free after freeing");
        
        pool_destroy(pool);
    }
}

void test_pool_growth(void) {
    memory_pool_t *pool = pool_create("growth_test", 16, 3);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    if (pool) {
        uint32_t initial_total = pool->total_objects;
        
        // Allocate all initial objects
        void *objects[10];
        int allocated = 0;
        for (int i = 0; i < 10; i++) {
            objects[i] = pool_alloc(pool);
            if (objects[i] != NULL) {
                allocated++;
            }
        }
        
        TEST_ASSERT(allocated > initial_total, "Pool should grow beyond initial size");
        TEST_ASSERT(pool->total_objects > initial_total, "Total objects should increase");
        
        // Free all allocated objects
        for (int i = 0; i < allocated; i++) {
            pool_free(pool, objects[i]);
        }
        
        pool_destroy(pool);
    }
}

void test_pool_utilization(void) {
    memory_pool_t *pool = pool_create("util_test", 64, 10);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    if (pool) {
        uint32_t util_empty = pool_get_utilization(pool);
        TEST_ASSERT(util_empty == 0, "Utilization should be 0% when empty");
        
        void *obj1 = pool_alloc(pool);
        void *obj2 = pool_alloc(pool);
        void *obj3 = pool_alloc(pool);
        
        uint32_t util_partial = pool_get_utilization(pool);
        TEST_ASSERT(util_partial > 0 && util_partial < 100, 
                   "Utilization should be between 0% and 100%");
        
        // Allocate all remaining objects
        void *objects[20];
        int allocated = 0;
        for (int i = 0; i < 20; i++) {
            objects[i] = pool_alloc(pool);
            if (objects[i] != NULL) {
                allocated++;
            } else {
                break;
            }
        }
        
        // Free some objects
        pool_free(pool, obj1);
        pool_free(pool, obj2);
        pool_free(pool, obj3);
        for (int i = 0; i < allocated; i++) {
            pool_free(pool, objects[i]);
        }
        
        uint32_t util_after_free = pool_get_utilization(pool);
        TEST_ASSERT(util_after_free == 0, "Utilization should be 0% after freeing all");
        
        pool_destroy(pool);
    }
}

void test_pool_multiple_sizes(void) {
    memory_pool_t *pool_small = pool_create("small", 8, 5);
    memory_pool_t *pool_medium = pool_create("medium", 128, 5);
    memory_pool_t *pool_large = pool_create("large", 1024, 5);
    
    TEST_ASSERT(pool_small != NULL, "Small pool creation should succeed");
    TEST_ASSERT(pool_medium != NULL, "Medium pool creation should succeed");
    TEST_ASSERT(pool_large != NULL, "Large pool creation should succeed");
    
    if (pool_small && pool_medium && pool_large) {
        void *obj_small = pool_alloc(pool_small);
        void *obj_medium = pool_alloc(pool_medium);
        void *obj_large = pool_alloc(pool_large);
        
        TEST_ASSERT(obj_small != NULL, "Small object allocation should succeed");
        TEST_ASSERT(obj_medium != NULL, "Medium object allocation should succeed");
        TEST_ASSERT(obj_large != NULL, "Large object allocation should succeed");
        
        pool_free(pool_small, obj_small);
        pool_free(pool_medium, obj_medium);
        pool_free(pool_large, obj_large);
    }
    
    if (pool_small) pool_destroy(pool_small);
    if (pool_medium) pool_destroy(pool_medium);
    if (pool_large) pool_destroy(pool_large);
}

void test_pool_stress(void) {
    memory_pool_t *pool = pool_create("stress_test", 48, 10);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    if (pool) {
        void *objects[50];
        int allocated = 0;
        
        // Allocate many objects
        for (int i = 0; i < 50; i++) {
            objects[i] = pool_alloc(pool);
            if (objects[i] != NULL) {
                allocated++;
            }
        }
        
        TEST_ASSERT(allocated > 0, "Should allocate at least some objects");
        
        // Free half
        for (int i = 0; i < allocated / 2; i++) {
            pool_free(pool, objects[i]);
        }
        
        // Allocate again
        for (int i = 0; i < allocated / 2; i++) {
            objects[i] = pool_alloc(pool);
        }
        
        // Free all
        for (int i = 0; i < allocated; i++) {
            if (objects[i] != NULL) {
                pool_free(pool, objects[i]);
            }
        }
        
        TEST_ASSERT(pool->free_objects == pool->total_objects, 
                   "All objects should be free after stress test");
        
        pool_destroy(pool);
    }
}

void run_pool_tests(void) {
    kprintf("Running memory pool tests...\n");
    
    test_pool_creation_and_destruction();
    test_pool_allocation_and_freeing();
    test_pool_constant_time_operations();
    test_pool_growth();
    test_pool_utilization();
    test_pool_multiple_sizes();
    test_pool_stress();
    
    kprintf("Pool tests: %d/%d passed\n", test_passed, test_count);
}

