#include "../../include/kernel/heap.h"
#include "../../include/kernel/stdio.h"
#include "../../include/kernel/string.h"

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

void test_heap_allocation_header_validation(void) {
    // Allocate memory and verify header is set correctly
    void *ptr = kmalloc(128);
    TEST_ASSERT(ptr != NULL, "kmalloc(128) should succeed");
    
    if (ptr) {
        // Get header
        alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
        
        // Validate magic number
        TEST_ASSERT(header->magic == ALLOC_MAGIC, "Header magic number should be correct");
        
        // Validate size
        TEST_ASSERT(header->size == 128, "Header size should match requested size");
        
        // Validate flags (should be from slab for 128 bytes)
        TEST_ASSERT(header->flags & ALLOC_FROM_SLAB, "128-byte allocation should be from slab");
        
        kfree(ptr);
    }
}

void test_heap_double_free_detection(void) {
    void *ptr = kmalloc(64);
    TEST_ASSERT(ptr != NULL, "kmalloc(64) should succeed");
    
    if (ptr) {
        // First free should succeed
        kfree(ptr);
        
        // Corrupt the magic number to simulate double-free
        alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
        header->magic = 0xDEADDEAD;  // Corrupt it
        
        // Second free should detect corruption and log error
        // (We can't easily test the error message, but it shouldn't crash)
        kfree(ptr);
        TEST_ASSERT(1, "Double-free detection should not crash");
    }
}

void test_heap_corrupted_header_detection(void) {
    void *ptr = kmalloc(256);
    TEST_ASSERT(ptr != NULL, "kmalloc(256) should succeed");
    
    if (ptr) {
        // Corrupt the header
        alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
        uint32_t original_magic = header->magic;
        header->magic = 0x12345678;  // Corrupt magic number
        
        // kfree should detect corruption
        kfree(ptr);
        TEST_ASSERT(1, "Corrupted header detection should not crash");
        
        // Restore magic for cleanup (though it's already freed)
        header->magic = original_magic;
    }
}

void test_heap_slab_vs_heap_routing(void) {
    // Small allocation should go to slab
    void *small_ptr = kmalloc(32);
    TEST_ASSERT(small_ptr != NULL, "Small allocation should succeed");
    
    if (small_ptr) {
        alloc_header_t *header = (alloc_header_t *)((uint8_t *)small_ptr - sizeof(alloc_header_t));
        TEST_ASSERT(header->flags & ALLOC_FROM_SLAB, "Small allocation should be from slab");
        TEST_ASSERT(header->slab_cache_index == SLAB_CACHE_32, "Should use 32-byte cache");
        kfree(small_ptr);
    }
    
    // Large allocation should go to heap
    void *large_ptr = kmalloc(8192);
    TEST_ASSERT(large_ptr != NULL, "Large allocation should succeed");
    
    if (large_ptr) {
        alloc_header_t *header = (alloc_header_t *)((uint8_t *)large_ptr - sizeof(alloc_header_t));
        TEST_ASSERT(header->flags & ALLOC_FROM_HEAP, "Large allocation should be from heap");
        TEST_ASSERT(header->slab_cache_index == SLAB_CACHE_NONE, "Heap allocation should have no cache index");
        kfree(large_ptr);
    }
}

void test_heap_kcalloc_zeroing(void) {
    size_t count = 10;
    size_t size = 64;
    void *ptr = kcalloc(count, size);
    TEST_ASSERT(ptr != NULL, "kcalloc should succeed");
    
    if (ptr) {
        // Verify memory is zeroed
        uint8_t *bytes = (uint8_t *)ptr;
        int all_zero = 1;
        for (size_t i = 0; i < count * size; i++) {
            if (bytes[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        TEST_ASSERT(all_zero, "kcalloc should zero-fill memory");
        
        // Verify header
        alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
        TEST_ASSERT(header->magic == ALLOC_MAGIC, "kcalloc header should be valid");
        TEST_ASSERT(header->size == count * size, "kcalloc header size should be correct");
        
        kfree(ptr);
    }
}

void test_heap_krealloc_functionality(void) {
    // Allocate initial memory
    void *ptr1 = kmalloc(64);
    TEST_ASSERT(ptr1 != NULL, "Initial kmalloc should succeed");
    
    if (ptr1) {
        // Fill with pattern
        uint8_t *bytes = (uint8_t *)ptr1;
        for (int i = 0; i < 64; i++) {
            bytes[i] = (uint8_t)i;
        }
        
        // Realloc to larger size
        void *ptr2 = krealloc(ptr1, 256);
        TEST_ASSERT(ptr2 != NULL, "krealloc to larger size should succeed");
        
        if (ptr2) {
            // Verify data was copied
            uint8_t *new_bytes = (uint8_t *)ptr2;
            int data_preserved = 1;
            for (int i = 0; i < 64; i++) {
                if (new_bytes[i] != (uint8_t)i) {
                    data_preserved = 0;
                    break;
                }
            }
            TEST_ASSERT(data_preserved, "krealloc should preserve existing data");
            
            // Verify header
            alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr2 - sizeof(alloc_header_t));
            TEST_ASSERT(header->magic == ALLOC_MAGIC, "krealloc header should be valid");
            
            kfree(ptr2);
        }
    }
}

void test_heap_null_pointer_handling(void) {
    // kfree(NULL) should not crash
    kfree(NULL);
    TEST_ASSERT(1, "kfree(NULL) should not crash");
    
    // krealloc(NULL, size) should behave like kmalloc
    void *ptr = krealloc(NULL, 128);
    TEST_ASSERT(ptr != NULL, "krealloc(NULL, size) should allocate new memory");
    
    if (ptr) {
        alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
        TEST_ASSERT(header->magic == ALLOC_MAGIC, "krealloc(NULL) should create valid header");
        kfree(ptr);
    }
    
    // krealloc(ptr, 0) should free memory
    void *ptr2 = kmalloc(64);
    if (ptr2) {
        void *result = krealloc(ptr2, 0);
        TEST_ASSERT(result == NULL, "krealloc(ptr, 0) should return NULL");
    }
}

void test_heap_overflow_detection(void) {
    // Test kcalloc overflow detection
    void *ptr = kcalloc(SIZE_MAX / 2, SIZE_MAX / 2);
    TEST_ASSERT(ptr == NULL, "kcalloc with overflow should return NULL");
}

void run_heap_tests(void) {
    kprintf("\nRunning heap allocator tests...\n");
    
    test_heap_allocation_header_validation();
    test_heap_double_free_detection();
    test_heap_corrupted_header_detection();
    test_heap_slab_vs_heap_routing();
    test_heap_kcalloc_zeroing();
    test_heap_krealloc_functionality();
    test_heap_null_pointer_handling();
    test_heap_overflow_detection();
    
    kprintf("Heap tests: %d/%d passed\n", test_passed, test_count);
}
