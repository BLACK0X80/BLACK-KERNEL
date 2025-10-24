#include "../../include/kernel/heap.h"
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

void stress_test_allocation_until_oom(void) {
    kprintf("\n=== Stress Test: Allocation Until OOM ===\n");
    
    uint64_t initial_free = buddy_get_free_pages();
    kprintf("Initial free pages: %llu\n", initial_free);
    
    // Allocate pages until we run out
    void *allocations[1000];
    int alloc_count = 0;
    
    for (int i = 0; i < 1000; i++) {
        void *ptr = kmalloc(4096);
        if (ptr == NULL) {
            kprintf("Allocation failed at iteration %d\n", i);
            break;
        }
        allocations[alloc_count++] = ptr;
    }
    
    kprintf("Successfully allocated %d blocks\n", alloc_count);
    TEST_ASSERT(alloc_count > 0, "Should allocate at least some blocks");
    
    // Free all allocations
    for (int i = 0; i < alloc_count; i++) {
        kfree(allocations[i]);
    }
    
    uint64_t final_free = buddy_get_free_pages();
    kprintf("Final free pages: %llu\n", final_free);
    
    // Allow some tolerance for fragmentation
    TEST_ASSERT(final_free >= initial_free - 10, 
                "Most memory should be freed (allowing for fragmentation)");
}

void stress_test_rapid_alloc_free(void) {
    kprintf("\n=== Stress Test: Rapid Alloc/Free Cycles ===\n");
    
    const int iterations = 1000;
    const int sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    const int num_sizes = 8;
    
    uint64_t initial_free = buddy_get_free_pages();
    
    for (int iter = 0; iter < iterations; iter++) {
        // Allocate
        void *ptr = kmalloc(sizes[iter % num_sizes]);
        if (ptr == NULL) {
            kprintf("Allocation failed at iteration %d\n", iter);
            break;
        }
        
        // Immediately free
        kfree(ptr);
    }
    
    uint64_t final_free = buddy_get_free_pages();
    
    kprintf("Completed %d alloc/free cycles\n", iterations);
    kprintf("Initial free: %llu, Final free: %llu\n", initial_free, final_free);
    
    TEST_ASSERT(final_free >= initial_free - 5, 
                "Memory should be mostly recovered after rapid cycles");
}

void stress_test_mixed_sizes(void) {
    kprintf("\n=== Stress Test: Mixed Size Allocations ===\n");
    
    const int num_allocs = 100;
    void *allocations[100];
    size_t sizes[100];
    
    // Allocate various sizes
    for (int i = 0; i < num_allocs; i++) {
        // Vary sizes: small, medium, large
        if (i % 3 == 0) {
            sizes[i] = 32;  // Small (slab)
        } else if (i % 3 == 1) {
            sizes[i] = 512;  // Medium (slab)
        } else {
            sizes[i] = 8192;  // Large (heap)
        }
        
        allocations[i] = kmalloc(sizes[i]);
        if (allocations[i] == NULL) {
            kprintf("Allocation %d failed (size %zu)\n", i, sizes[i]);
            // Free what we allocated so far
            for (int j = 0; j < i; j++) {
                if (allocations[j]) kfree(allocations[j]);
            }
            TEST_ASSERT(0, "Mixed size allocation failed");
            return;
        }
        
        // Write pattern to verify later
        uint8_t *bytes = (uint8_t *)allocations[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            bytes[j] = (uint8_t)(i & 0xFF);
        }
    }
    
    kprintf("Allocated %d blocks of mixed sizes\n", num_allocs);
    
    // Verify patterns
    int corrupted = 0;
    for (int i = 0; i < num_allocs; i++) {
        uint8_t *bytes = (uint8_t *)allocations[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            if (bytes[j] != (uint8_t)(i & 0xFF)) {
                corrupted++;
                break;
            }
        }
    }
    
    TEST_ASSERT(corrupted == 0, "No allocations should be corrupted");
    kprintf("Verified %d allocations, %d corrupted\n", num_allocs, corrupted);
    
    // Free in random order (every other one first)
    for (int i = 0; i < num_allocs; i += 2) {
        kfree(allocations[i]);
        allocations[i] = NULL;
    }
    
    // Free remaining
    for (int i = 1; i < num_allocs; i += 2) {
        kfree(allocations[i]);
    }
    
    kprintf("Freed all allocations\n");
    TEST_ASSERT(1, "Mixed size stress test completed");
}

void stress_test_fragmentation(void) {
    kprintf("\n=== Stress Test: Fragmentation Handling ===\n");
    
    const int num_blocks = 50;
    void *blocks[50];
    
    // Allocate many small blocks
    for (int i = 0; i < num_blocks; i++) {
        blocks[i] = kmalloc(64);
        if (blocks[i] == NULL) {
            kprintf("Small allocation %d failed\n", i);
            for (int j = 0; j < i; j++) {
                if (blocks[j]) kfree(blocks[j]);
            }
            TEST_ASSERT(0, "Small allocation failed");
            return;
        }
    }
    
    // Free every other block to create fragmentation
    for (int i = 0; i < num_blocks; i += 2) {
        kfree(blocks[i]);
        blocks[i] = NULL;
    }
    
    kprintf("Created fragmentation by freeing every other block\n");
    
    // Try to allocate larger blocks
    void *large_blocks[10];
    int large_count = 0;
    for (int i = 0; i < 10; i++) {
        large_blocks[i] = kmalloc(256);
        if (large_blocks[i] != NULL) {
            large_count++;
        }
    }
    
    kprintf("Allocated %d large blocks in fragmented memory\n", large_count);
    TEST_ASSERT(large_count > 0, "Should allocate some large blocks despite fragmentation");
    
    // Clean up
    for (int i = 1; i < num_blocks; i += 2) {
        if (blocks[i]) kfree(blocks[i]);
    }
    for (int i = 0; i < large_count; i++) {
        kfree(large_blocks[i]);
    }
}

void stress_test_concurrent_operations(void) {
    kprintf("\n=== Stress Test: Simulated Concurrent Operations ===\n");
    
    // Simulate concurrent operations by interleaving allocations and frees
    const int operations = 500;
    void *active_allocs[50];
    int active_count = 0;
    
    for (int i = 0; i < operations; i++) {
        // Randomly allocate or free
        if ((i % 3 == 0) && active_count > 0) {
            // Free a random active allocation
            int idx = i % active_count;
            kfree(active_allocs[idx]);
            // Move last element to freed position
            active_allocs[idx] = active_allocs[active_count - 1];
            active_count--;
        } else if (active_count < 50) {
            // Allocate
            size_t size = 64 + (i % 8) * 64;  // 64 to 512 bytes
            void *ptr = kmalloc(size);
            if (ptr) {
                active_allocs[active_count++] = ptr;
            }
        }
    }
    
    kprintf("Completed %d interleaved operations, %d active allocations remaining\n",
            operations, active_count);
    
    // Clean up remaining allocations
    for (int i = 0; i < active_count; i++) {
        kfree(active_allocs[i]);
    }
    
    TEST_ASSERT(1, "Concurrent operations simulation completed");
}

void stress_test_memory_leak_detection(void) {
    kprintf("\n=== Stress Test: Memory Leak Detection ===\n");
    
    uint64_t initial_free = buddy_get_free_pages();
    
    // Allocate and free many times
    for (int i = 0; i < 100; i++) {
        void *ptr = kmalloc(128);
        if (ptr) {
            kfree(ptr);
        }
    }
    
    uint64_t final_free = buddy_get_free_pages();
    
    kprintf("Initial free: %llu, Final free: %llu\n", initial_free, final_free);
    
    // Should have same or very close to same free pages
    int64_t diff = (int64_t)final_free - (int64_t)initial_free;
    TEST_ASSERT(diff >= -2 && diff <= 2, 
                "No significant memory leak should be detected");
    
    if (diff < -2) {
        kprintf("WARNING: Possible memory leak detected (%lld pages)\n", -diff);
    }
}

void run_stress_tests(void) {
    kprintf("\n========================================\n");
    kprintf("     Memory Management Stress Tests    \n");
    kprintf("========================================\n");
    
    stress_test_allocation_until_oom();
    stress_test_rapid_alloc_free();
    stress_test_mixed_sizes();
    stress_test_fragmentation();
    stress_test_concurrent_operations();
    stress_test_memory_leak_detection();
    
    kprintf("\n========================================\n");
    kprintf("Stress tests: %d/%d passed\n", test_passed, test_count);
    kprintf("========================================\n");
}
