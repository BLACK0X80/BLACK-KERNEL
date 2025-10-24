#include "../../include/mm/cow.h"
#include "../../include/mm/page_cache.h"
#include "../../include/kernel/stdio.h"

// Simple cycle counter (x86-64 RDTSC)
static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void benchmark_cow_hash_function(void) {
    kprintf("\n=== COW Hash Function Benchmark ===\n");
    
    const int iterations = 10000;
    uint64_t test_addresses[100];
    
    // Generate test addresses
    for (int i = 0; i < 100; i++) {
        test_addresses[i] = 0x100000 + (i * 0x1000);
    }
    
    // Benchmark new hash function (bitwise AND)
    uint64_t start = read_tsc();
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < 100; i++) {
            // Simulate hash calculation
            uint64_t phys_addr = test_addresses[i];
            phys_addr &= ~0xFFFULL;
            volatile uint32_t hash = (uint32_t)((phys_addr >> 12) & COW_HASH_MASK);
            (void)hash;  // Prevent optimization
        }
    }
    uint64_t end = read_tsc();
    uint64_t cycles_new = end - start;
    
    kprintf("New hash (bitwise AND): %llu cycles for %d iterations\n", 
            cycles_new, iterations * 100);
    kprintf("Average: %llu cycles per hash\n", cycles_new / (iterations * 100));
    
    // Test distribution
    int buckets[COW_HASH_SIZE] = {0};
    for (int i = 0; i < 1000; i++) {
        uint64_t addr = 0x100000 + (i * 0x1000);
        addr &= ~0xFFFULL;
        uint32_t hash = (uint32_t)((addr >> 12) & COW_HASH_MASK);
        if (hash < COW_HASH_SIZE) {
            buckets[hash]++;
        }
    }
    
    // Calculate distribution statistics
    int min_count = buckets[0];
    int max_count = buckets[0];
    for (int i = 1; i < COW_HASH_SIZE; i++) {
        if (buckets[i] < min_count) min_count = buckets[i];
        if (buckets[i] > max_count) max_count = buckets[i];
    }
    
    kprintf("Distribution: min=%d, max=%d (1000 addresses across %d buckets)\n",
            min_count, max_count, COW_HASH_SIZE);
}

void benchmark_page_cache_hash_function(void) {
    kprintf("\n=== Page Cache Hash Function Benchmark ===\n");
    
    const int iterations = 10000;
    
    // Benchmark new hash function
    uint64_t start = read_tsc();
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < 100; i++) {
            uint64_t file_id = i;
            uint64_t offset = iter * 0x1000;
            
            // Simulate new hash calculation
            uint64_t hash = file_id ^ (offset >> 12);
            hash = hash * 2654435761ULL;
            volatile uint32_t result = (uint32_t)(hash & (1024 - 1));
            (void)result;
        }
    }
    uint64_t end = read_tsc();
    uint64_t cycles_new = end - start;
    
    kprintf("New hash (XOR + golden ratio + AND): %llu cycles for %d iterations\n",
            cycles_new, iterations * 100);
    kprintf("Average: %llu cycles per hash\n", cycles_new / (iterations * 100));
    
    // Test distribution
    int buckets[1024] = {0};
    for (int file = 0; file < 10; file++) {
        for (int off = 0; off < 100; off++) {
            uint64_t hash = file ^ (off * 0x1000 >> 12);
            hash = hash * 2654435761ULL;
            uint32_t result = (uint32_t)(hash & 1023);
            buckets[result]++;
        }
    }
    
    // Calculate distribution statistics
    int min_count = buckets[0];
    int max_count = buckets[0];
    for (int i = 1; i < 1024; i++) {
        if (buckets[i] < min_count) min_count = buckets[i];
        if (buckets[i] > max_count) max_count = buckets[i];
    }
    
    kprintf("Distribution: min=%d, max=%d (1000 hashes across 1024 buckets)\n",
            min_count, max_count);
}

void benchmark_comparison(void) {
    kprintf("\n=== Performance Comparison ===\n");
    
    const int iterations = 100000;
    
    // Benchmark modulo operation (old method)
    uint64_t start_mod = read_tsc();
    for (int i = 0; i < iterations; i++) {
        volatile uint32_t result = (uint32_t)(i % 1024);
        (void)result;
    }
    uint64_t end_mod = read_tsc();
    uint64_t cycles_mod = end_mod - start_mod;
    
    // Benchmark bitwise AND (new method)
    uint64_t start_and = read_tsc();
    for (int i = 0; i < iterations; i++) {
        volatile uint32_t result = (uint32_t)(i & 1023);
        (void)result;
    }
    uint64_t end_and = read_tsc();
    uint64_t cycles_and = end_and - start_and;
    
    kprintf("Modulo operation: %llu cycles for %d iterations\n", cycles_mod, iterations);
    kprintf("Bitwise AND:      %llu cycles for %d iterations\n", cycles_and, iterations);
    kprintf("Speedup: %.2fx faster\n", (double)cycles_mod / (double)cycles_and);
}

void run_performance_benchmarks(void) {
    kprintf("\n========================================\n");
    kprintf("  Memory Management Performance Tests  \n");
    kprintf("========================================\n");
    
    benchmark_cow_hash_function();
    benchmark_page_cache_hash_function();
    benchmark_comparison();
    
    kprintf("\n========================================\n");
}
