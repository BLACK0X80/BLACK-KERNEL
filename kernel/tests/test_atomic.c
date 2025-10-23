#include "../../include/kernel/atomic.h"
#include "../../include/kernel/types.h"
#include <stdio.h>
#include <assert.h>

void test_atomic_compare_and_swap(void) {
    kprintf("Testing atomic_compare_and_swap...\n");
    
    volatile uint32_t value = 10;
    uint32_t prev;
    
    // Test successful swap
    prev = atomic_compare_and_swap(&value, 10, 20);
    assert(prev == 10);
    assert(value == 20);
    
    // Test failed swap (expected value doesn't match)
    prev = atomic_compare_and_swap(&value, 10, 30);
    assert(prev == 20);
    assert(value == 20); // Value should remain unchanged
    
    // Test successful swap again
    prev = atomic_compare_and_swap(&value, 20, 0);
    assert(prev == 20);
    assert(value == 0);
    
    kprintf("  atomic_compare_and_swap: PASSED\n");
}

void test_atomic_fetch_and_add(void) {
    kprintf("Testing atomic_fetch_and_add...\n");
    
    volatile uint32_t value = 0;
    uint32_t prev;
    
    // Test adding to zero
    prev = atomic_fetch_and_add(&value, 5);
    assert(prev == 0);
    assert(value == 5);
    
    // Test adding to non-zero
    prev = atomic_fetch_and_add(&value, 10);
    assert(prev == 5);
    assert(value == 15);
    
    // Test adding 1
    prev = atomic_fetch_and_add(&value, 1);
    assert(prev == 15);
    assert(value == 16);
    
    // Test adding large value
    prev = atomic_fetch_and_add(&value, 1000);
    assert(prev == 16);
    assert(value == 1016);
    
    kprintf("  atomic_fetch_and_add: PASSED\n");
}

void test_atomic_store_load(void) {
    kprintf("Testing atomic_store and atomic_load...\n");
    
    volatile uint32_t value = 0;
    
    // Test store and load
    atomic_store(&value, 42);
    uint32_t loaded = atomic_load(&value);
    assert(loaded == 42);
    
    // Test with different values
    atomic_store(&value, 0xDEADBEEF);
    loaded = atomic_load(&value);
    assert(loaded == 0xDEADBEEF);
    
    atomic_store(&value, 0);
    loaded = atomic_load(&value);
    assert(loaded == 0);
    
    kprintf("  atomic_store/load: PASSED\n");
}

void test_memory_barrier(void) {
    kprintf("Testing memory_barrier...\n");
    
    // Memory barrier doesn't have observable behavior in single-threaded test
    // but we can verify it compiles and executes without error
    volatile uint32_t a = 1;
    volatile uint32_t b = 2;
    
    atomic_store(&a, 10);
    memory_barrier();
    atomic_store(&b, 20);
    memory_barrier();
    
    assert(atomic_load(&a) == 10);
    assert(atomic_load(&b) == 20);
    
    kprintf("  memory_barrier: PASSED\n");
}

void run_atomic_tests(void) {
    kprintf("\n=== Atomic Operations Tests ===\n\n");
    
    test_atomic_compare_and_swap();
    test_atomic_fetch_and_add();
    test_atomic_store_load();
    test_memory_barrier();
    
    kprintf("\n=== All Atomic Tests Passed ===\n\n");
    return 0;
}

