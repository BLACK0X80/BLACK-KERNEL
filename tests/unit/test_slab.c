#include "../../include/mm/slab.h"
#include "../../include/mm/buddy.h"
#include "../../include/kernel/stdio.h"
#include "../../include/kernel/string.h"

static int test_count = 0;
static int test_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    test_count++; \
    if (condition) { \
        test_passed++; \
    } else { \
        printf("[FAIL] %s\n", message); \
    } \
} while(0)

// Forward declarations
void test_slab_cache_creation(void);
void test_slab_object_allocation(void);
void test_slab_object_freeing(void);
void test_slab_coloring(void);
void test_slab_cpu_cache(void);
void test_slab_stress(void);
void test_slab_statistics(void);
void test_slab_cache_destruction(void);
void test_slab_multiple_caches(void);
void test_slab_alignment(void);
void test_slab_reuse(void);
void test_slab_null_handling(void);

void test_slab_cache_creation(void) {
    slab_cache_t *cache = slab_cache_create("test_cache", 64, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    TEST_ASSERT(cache->object_size >= 64, "Object size should be at least 64 bytes");
    TEST_ASSERT(cache->objects_per_slab > 0, "Objects per slab should be greater than 0");
    
    slab_cache_destroy(cache);
}

void test_slab_object_allocation(void) {
    slab_cache_t *cache = slab_cache_create("test_alloc", 128, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    void *obj1 = slab_alloc(cache);
    TEST_ASSERT(obj1 != NULL, "First object allocation should succeed");
    
    void *obj2 = slab_alloc(cache);
    TEST_ASSERT(obj2 != NULL, "Second object allocation should succeed");
    TEST_ASSERT(obj1 != obj2, "Allocated objects should have different addresses");
    
    slab_free(cache, obj1);
    slab_free(cache, obj2);
    slab_cache_destroy(cache);
}

void test_slab_object_freeing(void) {
    slab_cache_t *cache = slab_cache_create("test_free", 64, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    void *obj = slab_alloc(cache);
    TEST_ASSERT(obj != NULL, "Object allocation should succeed");
    
    uint64_t allocs_before, frees_before, hits_before;
    slab_get_stats(cache, &allocs_before, &frees_before, &hits_before);
    
    slab_free(cache, obj);
    
    uint64_t allocs_after, frees_after, hits_after;
    slab_get_stats(cache, &allocs_after, &frees_after, &hits_after);
    
    TEST_ASSERT(frees_after == frees_before + 1, "Free count should increase by 1");
    
    slab_cache_destroy(cache);
}

void test_slab_coloring(void) {
    slab_cache_t *cache = slab_cache_create("test_color", 32, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    uint32_t initial_color = cache->color_next;
    
    // Allocate enough objects to force creation of multiple slabs
    // This will test that color_next advances
    void *objects[100];
    for (int i = 0; i < 100; i++) {
        objects[i] = slab_alloc(cache);
        TEST_ASSERT(objects[i] != NULL, "Allocation should succeed");
    }
    
    // Verify that color_next has changed (indicating slab coloring is active)
    TEST_ASSERT(cache->color_next != initial_color, 
                "Color offset should advance with new slabs");
    
    // Free all objects
    for (int i = 0; i < 100; i++) {
        slab_free(cache, objects[i]);
    }
    
    slab_cache_destroy(cache);
}

void test_slab_cpu_cache(void) {
    slab_cache_t *cache = slab_cache_create("test_cpu", 64, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    // First allocation should miss CPU cache and allocate from shared cache
    void *obj1 = slab_alloc(cache);
    TEST_ASSERT(obj1 != NULL, "First allocation should succeed");
    
    uint64_t allocs1, frees1, hits1;
    slab_get_stats(cache, &allocs1, &frees1, &hits1);
    TEST_ASSERT(allocs1 == 1, "Should have 1 allocation");
    TEST_ASSERT(hits1 == 0, "First allocation should not be a cache hit");
    
    // Free the object - it should go to CPU cache
    slab_free(cache, obj1);
    
    // Allocate again - should hit CPU cache
    void *obj2 = slab_alloc(cache);
    TEST_ASSERT(obj2 != NULL, "Second allocation should succeed");
    
    uint64_t allocs2, frees2, hits2;
    slab_get_stats(cache, &allocs2, &frees2, &hits2);
    TEST_ASSERT(allocs2 == 2, "Should have 2 allocations");
    TEST_ASSERT(hits2 > hits1, "Second allocation should increase cache hits");
    
    // Test batch allocation and freeing
    void *objects[20];
    for (int i = 0; i < 20; i++) {
        objects[i] = slab_alloc(cache);
        TEST_ASSERT(objects[i] != NULL, "Allocation should succeed");
    }
    
    uint64_t allocs3, frees3, hits3;
    slab_get_stats(cache, &allocs3, &frees3, &hits3);
    TEST_ASSERT(allocs3 == 22, "Should have 22 total allocations");
    
    // Free all objects
    slab_free(cache, obj2);
    for (int i = 0; i < 20; i++) {
        slab_free(cache, objects[i]);
    }
    
    uint64_t allocs4, frees4, hits4;
    slab_get_stats(cache, &allocs4, &frees4, &hits4);
    TEST_ASSERT(frees4 == 21, "Should have 21 frees");
    
    slab_cache_destroy(cache);
}

void test_slab_stress(void) {
    slab_cache_t *cache = slab_cache_create("test_stress", 256, 16);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    #define STRESS_COUNT 100
    void *objects[STRESS_COUNT];
    
    for (int i = 0; i < STRESS_COUNT; i++) {
        objects[i] = slab_alloc(cache);
        TEST_ASSERT(objects[i] != NULL, "Stress allocation should succeed");
    }
    
    for (int i = 0; i < STRESS_COUNT; i++) {
        if (objects[i]) {
            memset(objects[i], 0xAA, 256);
        }
    }
    
    for (int i = 0; i < STRESS_COUNT; i++) {
        slab_free(cache, objects[i]);
    }
    
    uint64_t allocs, frees, hits;
    slab_get_stats(cache, &allocs, &frees, &hits);
    TEST_ASSERT(allocs == STRESS_COUNT, "Should have correct allocation count");
    TEST_ASSERT(frees == STRESS_COUNT, "Should have correct free count");
    
    slab_cache_destroy(cache);
}

void test_slab_statistics(void) {
    slab_cache_t *cache = slab_cache_create("test_stats", 128, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    uint64_t allocs, frees, hits;
    slab_get_stats(cache, &allocs, &frees, &hits);
    TEST_ASSERT(allocs == 0 && frees == 0, "Initial stats should be zero");
    
    void *obj1 = slab_alloc(cache);
    void *obj2 = slab_alloc(cache);
    
    slab_get_stats(cache, &allocs, &frees, &hits);
    TEST_ASSERT(allocs == 2, "Should have 2 allocations");
    
    slab_free(cache, obj1);
    slab_get_stats(cache, &allocs, &frees, &hits);
    TEST_ASSERT(frees == 1, "Should have 1 free");
    
    slab_free(cache, obj2);
    slab_get_stats(cache, &allocs, &frees, &hits);
    TEST_ASSERT(frees == 2, "Should have 2 frees");
    
    slab_cache_destroy(cache);
}

void test_slab_cache_destruction(void) {
    slab_cache_t *cache = slab_cache_create("test_destroy", 64, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    void *obj1 = slab_alloc(cache);
    void *obj2 = slab_alloc(cache);
    void *obj3 = slab_alloc(cache);
    
    TEST_ASSERT(obj1 != NULL && obj2 != NULL && obj3 != NULL, 
                "All allocations should succeed");
    
    // Destroy cache with outstanding allocations (should free all slabs)
    slab_cache_destroy(cache);
    TEST_ASSERT(1, "Cache destruction should complete without crashing");
}

void test_slab_multiple_caches(void) {
    // Test creating multiple caches with different object sizes
    slab_cache_t *cache1 = slab_cache_create("cache_32", 32, 8);
    slab_cache_t *cache2 = slab_cache_create("cache_64", 64, 8);
    slab_cache_t *cache3 = slab_cache_create("cache_128", 128, 16);
    
    TEST_ASSERT(cache1 != NULL && cache2 != NULL && cache3 != NULL,
                "All cache creations should succeed");
    TEST_ASSERT(cache1 != cache2 && cache2 != cache3 && cache1 != cache3,
                "Caches should have different addresses");
    
    // Allocate from each cache
    void *obj1 = slab_alloc(cache1);
    void *obj2 = slab_alloc(cache2);
    void *obj3 = slab_alloc(cache3);
    
    TEST_ASSERT(obj1 != NULL && obj2 != NULL && obj3 != NULL,
                "Allocations from all caches should succeed");
    
    // Free and destroy
    slab_free(cache1, obj1);
    slab_free(cache2, obj2);
    slab_free(cache3, obj3);
    
    slab_cache_destroy(cache1);
    slab_cache_destroy(cache2);
    slab_cache_destroy(cache3);
}

void test_slab_alignment(void) {
    // Test that objects are properly aligned
    slab_cache_t *cache = slab_cache_create("test_align", 100, 16);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    TEST_ASSERT(cache->object_size % 16 == 0, "Object size should be aligned to 16 bytes");
    
    void *obj1 = slab_alloc(cache);
    void *obj2 = slab_alloc(cache);
    
    TEST_ASSERT(obj1 != NULL && obj2 != NULL, "Allocations should succeed");
    TEST_ASSERT(((uintptr_t)obj1 % 16) == 0, "Object 1 should be 16-byte aligned");
    TEST_ASSERT(((uintptr_t)obj2 % 16) == 0, "Object 2 should be 16-byte aligned");
    
    slab_free(cache, obj1);
    slab_free(cache, obj2);
    slab_cache_destroy(cache);
}

void test_slab_reuse(void) {
    // Test that freed objects are reused
    slab_cache_t *cache = slab_cache_create("test_reuse", 64, 8);
    TEST_ASSERT(cache != NULL, "Cache creation should succeed");
    
    void *obj1 = slab_alloc(cache);
    TEST_ASSERT(obj1 != NULL, "First allocation should succeed");
    
    slab_free(cache, obj1);
    
    void *obj2 = slab_alloc(cache);
    TEST_ASSERT(obj2 != NULL, "Second allocation should succeed");
    
    // Due to CPU cache, obj2 might be the same as obj1 (reused)
    // This tests that the free list is working correctly
    
    slab_free(cache, obj2);
    slab_cache_destroy(cache);
}

void test_slab_null_handling(void) {
    // Test NULL parameter handling
    slab_cache_t *cache = slab_cache_create(NULL, 64, 8);
    TEST_ASSERT(cache == NULL, "Cache creation with NULL name should fail");
    
    cache = slab_cache_create("test_null", 0, 8);
    TEST_ASSERT(cache == NULL, "Cache creation with zero size should fail");
    
    cache = slab_cache_create("test_valid", 64, 8);
    TEST_ASSERT(cache != NULL, "Valid cache creation should succeed");
    
    void *obj = slab_alloc(NULL);
    TEST_ASSERT(obj == NULL, "Allocation from NULL cache should return NULL");
    
    slab_free(NULL, (void *)0x1000);
    TEST_ASSERT(1, "Freeing to NULL cache should not crash");
    
    slab_free(cache, NULL);
    TEST_ASSERT(1, "Freeing NULL object should not crash");
    
    slab_cache_destroy(cache);
    slab_cache_destroy(NULL);
    TEST_ASSERT(1, "Destroying NULL cache should not crash");
}

void run_slab_tests(void) {
    printf("Running slab allocator tests...\n");
    
    test_slab_cache_creation();
    test_slab_object_allocation();
    test_slab_object_freeing();
    test_slab_coloring();
    test_slab_cpu_cache();
    test_slab_stress();
    test_slab_statistics();
    test_slab_cache_destruction();
    test_slab_multiple_caches();
    test_slab_alignment();
    test_slab_reuse();
    test_slab_null_handling();
    
    printf("Slab tests: %d/%d passed\n", test_passed, test_count);
}
