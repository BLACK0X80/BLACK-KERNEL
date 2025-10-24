#pragma once

/**
 * Prometheus Kernel Debug Configuration
 * 
 * This file contains compile-time debug configuration for all kernel subsystems.
 * Debug logging can be enabled/disabled per module to reduce noise and overhead.
 * 
 * When DEBUG_MODE is disabled, all DEBUG_PRINT statements compile to no-ops,
 * resulting in zero runtime overhead for production builds.
 */

// ============================================================================
// Master Debug Switch
// ============================================================================

/**
 * DEBUG_MODE - Master switch for all debug logging
 * 
 * Set to 1 to enable debug logging globally.
 * Set to 0 to disable all debug logging (zero overhead).
 */
#define DEBUG_MODE 1

// ============================================================================
// Per-Module Debug Flags
// ============================================================================

/**
 * DEBUG_BUDDY - Buddy allocator debug logging
 * 
 * Logs:
 * - Zone selection decisions
 * - Invalid flag warnings
 * - Allocation and free operations
 * - Memory statistics
 */
#define DEBUG_BUDDY 1

/**
 * DEBUG_SLAB - Slab allocator debug logging
 * 
 * Logs:
 * - Cache creation and destruction
 * - Allocation and free operations
 * - Slab creation failures
 * - Object not found warnings
 */
#define DEBUG_SLAB 1

/**
 * DEBUG_COW - Copy-on-Write system debug logging
 * 
 * Logs:
 * - Page marking operations
 * - Page fault handling
 * - Page copy operations
 * - Reference count changes
 */
#define DEBUG_COW 1

/**
 * DEBUG_DEMAND_PAGING - Demand paging debug logging
 * 
 * Logs:
 * - Region registration
 * - Page fault handling
 * - Race condition detection
 * - Lock acquisition/release
 */
#define DEBUG_DEMAND_PAGING 1

/**
 * DEBUG_PAGE_CACHE - Page cache debug logging
 * 
 * Logs:
 * - Cache hits and misses
 * - Page insertion and eviction
 * - Hash function validation
 * - LRU operations
 */
#define DEBUG_PAGE_CACHE 1

// ============================================================================
// Debug Print Macro
// ============================================================================

#if DEBUG_MODE

/**
 * DEBUG_PRINT - Conditional debug print macro
 * 
 * Usage:
 *   DEBUG_PRINT(BUDDY, "Allocated %u pages from zone %u\n", pages, zone);
 * 
 * This will only print if DEBUG_BUDDY is enabled.
 * 
 * @param module Module name (BUDDY, SLAB, COW, DEMAND_PAGING, PAGE_CACHE)
 * @param ... Printf-style format string and arguments
 */
#define DEBUG_PRINT(module, ...) \
    do { \
        if (DEBUG_##module) { \
            kprintf("[DEBUG:%s] ", #module); \
            kprintf(__VA_ARGS__); \
        } \
    } while(0)

#else

/**
 * DEBUG_PRINT - No-op when DEBUG_MODE is disabled
 * 
 * Compiles to nothing, resulting in zero runtime overhead.
 */
#define DEBUG_PRINT(module, ...) do {} while(0)

#endif

// ============================================================================
// Usage Examples
// ============================================================================

/*
 * Example 1: Basic debug print
 * 
 *   DEBUG_PRINT(BUDDY, "Allocating %u pages\n", order);
 * 
 * Output (if DEBUG_BUDDY=1):
 *   [DEBUG:BUDDY] Allocating 4 pages
 * 
 * 
 * Example 2: Debug print with multiple arguments
 * 
 *   DEBUG_PRINT(COW, "Handling fault at 0x%llx, phys 0x%llx, refcount %u\n",
 *               virt_addr, phys_addr, ref_count);
 * 
 * Output (if DEBUG_COW=1):
 *   [DEBUG:COW] Handling fault at 0xFFFF800000001000, phys 0x100000, refcount 2
 * 
 * 
 * Example 3: Conditional compilation
 * 
 *   #if DEBUG_MODE && DEBUG_SLAB
 *   // Expensive debug-only code
 *   validate_slab_integrity(cache);
 *   #endif
 */

#endif // KERNEL_CONFIG_H
