# Memory Management Architecture

## Overview

Prometheus Kernel implements a multi-layered memory management system with the following components:

```
┌─────────────────────────────────────────────────────────────┐
│                    Kernel Subsystems                        │
└─────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Heap Manager │    │  COW System  │    │Demand Paging │
│  (kmalloc)   │    │              │    │              │
└──────────────┘    └──────────────┘    └──────────────┘
        │                   │                   │
        ▼                   ▼                   ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│    Slab      │    │  Page Cache  │    │     VMM      │
│  Allocator   │    │              │    │              │
└──────────────┘    └──────────────┘    └──────────────┘
        │                   │                   │
        └───────────────────┼───────────────────┘
                            ▼
                    ┌──────────────┐
                    │    Buddy     │
                    │  Allocator   │
                    └──────────────┘
                            │
                            ▼
                    ┌──────────────┐
                    │     PMM      │
                    │  (Wrapper)   │
                    └──────────────┘
```

## Components

### 1. Buddy Allocator (`kernel/mm/buddy.c`)

The buddy allocator is the foundation of physical memory management.

**Features:**
- Zone-based allocation (UNMOVABLE, RECLAIMABLE, MOVABLE)
- Power-of-2 block sizes (order 0 to 10)
- Coalescing of adjacent free blocks
- Per-zone statistics and debugging

**Zone Priority:**
- MOVABLE > RECLAIMABLE > UNMOVABLE
- MOVABLE pages can be relocated (future defragmentation)
- RECLAIMABLE pages can be freed under memory pressure
- UNMOVABLE pages are permanent kernel allocations

**API:**
```c
uint64_t buddy_alloc_pages(uint32_t order, buddy_zone_type_t zone);
void buddy_free_pages(uint64_t address, uint32_t order);
uint64_t buddy_alloc_pages_flags(uint32_t order, uint32_t flags);
```

### 2. Slab Allocator (`kernel/mm/slab.c`)

Object cache allocator for frequently allocated kernel objects.

**Features:**
- Per-cache object pools
- CPU-local caching for performance
- Cache coloring to reduce cache conflicts
- Slab states: full, partial, free

**Cache Sizes:**
- 16, 32, 64, 128, 256, 512, 1024, 2048 bytes
- Each size has a dedicated cache

**API:**
```c
slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align);
void *slab_alloc(slab_cache_t *cache);
void slab_free(slab_cache_t *cache, void *object);
```

### 3. Heap Manager (`kernel/mm/heap.c`)

General-purpose kernel memory allocator.

**Features:**
- Allocation header with magic number validation
- Automatic routing: small allocations → slab, large → heap
- Corruption detection via magic numbers
- Support for kmalloc, kcalloc, krealloc, kfree

**Allocation Header:**
```c
typedef struct alloc_header {
    uint32_t magic;              // 0xDEADBEEF
    uint32_t size;               // Requested size
    uint16_t flags;              // ALLOC_FROM_SLAB | ALLOC_FROM_HEAP
    uint8_t slab_cache_index;    // Cache index (0-7)
    uint8_t padding;
} alloc_header_t;
```

**API:**
```c
void *kmalloc(size_t size);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);
```

### 4. Copy-on-Write System (`kernel/mm/cow.c`)

Implements COW page sharing for fork() and memory efficiency.

**Features:**
- Reference counting for shared pages
- Hash table for fast lookup (1024 buckets)
- Optimized hash function (bitwise AND)
- Automatic page copying on write

**Hash Function:**
```c
hash = (phys_addr >> 12) & COW_HASH_MASK  // Fast bitwise AND
```

**API:**
```c
int cow_mark_page(page_table_t *pml4, uint64_t virt_addr);
int cow_handle_fault(page_table_t *pml4, uint64_t virt_addr);
void cow_increment_ref(uint64_t phys_addr);
void cow_decrement_ref(uint64_t phys_addr);
```

### 5. Demand Paging (`kernel/mm/demand_paging.c`)

Lazy page allocation system.

**Features:**
- Per-region page fault locks (prevents race conditions)
- Double-checked locking pattern
- Zero-fill support
- File-backed page support (future)

**Region Structure:**
```c
typedef struct vm_region {
    uint64_t start;
    uint64_t end;
    uint32_t flags;
    spinlock_t page_fault_lock;  // Per-region concurrency control
    struct vm_region *next;
} vm_region_t;
```

**API:**
```c
int demand_paging_register_region(page_table_t *pml4, uint64_t start, 
                                   uint64_t size, uint32_t flags);
int demand_paging_handle_fault(page_table_t *pml4, uint64_t virt_addr);
void demand_paging_unregister_region(page_table_t *pml4, uint64_t start);
```

### 6. Page Cache (`kernel/mm/page_cache.c`)

Cache for file-backed pages.

**Features:**
- LRU eviction policy
- Optimized hash function (XOR + golden ratio + bitwise AND)
- Power-of-2 hash table size (1024 buckets)
- Cache hit/miss statistics

**Hash Function:**
```c
hash = file_id ^ (offset >> 12);
hash = hash * 2654435761ULL;  // Golden ratio prime
result = hash & HASH_SIZE_MASK;  // Fast bitwise AND
```

**API:**
```c
uint64_t page_cache_lookup(uint64_t file_id, uint64_t offset);
int page_cache_insert(uint64_t file_id, uint64_t offset, uint64_t phys_addr);
void page_cache_evict_lru(void);
```

## Memory Allocation Flow

### Small Allocation (<4KB)

```
kmalloc(128)
    ↓
Check size → Use slab allocator
    ↓
Allocate from slab cache (kmalloc-128)
    ↓
Add allocation header
    ↓
Return pointer (after header)
```

### Large Allocation (≥4KB)

```
kmalloc(8192)
    ↓
Check size → Use heap allocator
    ↓
Find free block in heap
    ↓
Add allocation header
    ↓
Return pointer (after header)
```

### Free Operation

```
kfree(ptr)
    ↓
Get header at (ptr - sizeof(header))
    ↓
Validate magic number
    ↓
Check flags → SLAB or HEAP?
    ↓
Free to appropriate allocator
```

## Concurrency Control

### Locking Strategy

- **Buddy Allocator**: Per-zone spinlocks
- **Slab Allocator**: Per-cache spinlocks + CPU-local caching
- **COW System**: Global lock + per-reference locks
- **Demand Paging**: Per-region spinlocks (fine-grained)
- **Page Cache**: Global spinlock

### Race Condition Prevention

**Demand Paging Double-Checked Locking:**
```c
// First check (unlocked, fast path)
if (page_already_mapped) return 0;

// Acquire lock
spinlock_acquire(&region->page_fault_lock);

// Second check (locked, prevents race)
if (page_already_mapped) {
    spinlock_release(&region->page_fault_lock);
    return 0;
}

// Allocate and map page
// ...

spinlock_release(&region->page_fault_lock);
```

## Performance Optimizations

### 1. Bitwise Operations

Replace expensive modulo with bitwise AND:
```c
// Old: hash % 1024  (~20-40 cycles)
// New: hash & 1023  (~1 cycle)
```

### 2. CPU-Local Caching

Slab allocator maintains per-CPU object caches to reduce lock contention.

### 3. Allocation Headers

O(1) cache lookup on free using embedded cache index.

### 4. Zone-Based Allocation

Segregates allocations by mobility for future defragmentation.

## Debugging

### Debug Configuration

Enable/disable debug logging in `include/kernel/config.h`:
```c
#define DEBUG_MODE 1
#define DEBUG_BUDDY 1
#define DEBUG_SLAB 1
#define DEBUG_COW 1
#define DEBUG_DEMAND_PAGING 1
#define DEBUG_PAGE_CACHE 1
```

### Debug Macros

```c
DEBUG_PRINT(BUDDY, "Allocated %u pages from zone %u\n", pages, zone);
```

### Error Detection

- **Magic Number Validation**: Detects corruption and double-frees
- **Pointer Validation**: All pointers checked before dereferencing
- **Lock Verification**: Ensures locks are always released

## Testing

### Test Suite

- `kernel/tests/test_buddy.c` - Buddy allocator tests
- `kernel/tests/test_heap.c` - Heap manager tests
- `kernel/tests/test_cow.c` - COW system tests
- `kernel/tests/test_demand_paging.c` - Demand paging tests
- `kernel/tests/test_performance.c` - Performance benchmarks
- `kernel/tests/test_stress.c` - Stress tests

### Running Tests

```bash
# Build and run all tests
make test

# Run specific test
./run_test.sh test_buddy
```

## Design Decisions

### Why Allocation Headers?

- **Problem**: kfree() couldn't determine allocation source (slab vs heap)
- **Solution**: Prepend header with metadata
- **Trade-off**: 12 bytes overhead per allocation
- **Benefit**: O(1) cache lookup, corruption detection

### Why Per-Region Locks?

- **Problem**: Global lock caused contention on concurrent page faults
- **Solution**: Per-region spinlocks
- **Trade-off**: More memory per region
- **Benefit**: Fine-grained concurrency, better scalability

### Why Bitwise AND for Hashing?

- **Problem**: Modulo operation is slow (20-40 cycles)
- **Solution**: Use bitwise AND with power-of-2 sizes
- **Trade-off**: Hash table size must be power of 2
- **Benefit**: 20-40x faster hash computation

## Future Enhancements

1. **OOM Killer**: Graceful handling of memory exhaustion
2. **Memory Defragmentation**: Compact MOVABLE zone
3. **NUMA Awareness**: Per-node allocation policies
4. **Lock-Free Paths**: Reduce contention on hot paths
5. **Memory Accounting**: Per-process usage tracking
6. **Slab Reclaim**: Free empty slabs under pressure

## References

- Buddy System: Knuth, The Art of Computer Programming, Vol. 1
- Slab Allocator: Bonwick, "The Slab Allocator: An Object-Caching Kernel Memory Allocator"
- COW: Unix fork() implementation
- Demand Paging: Tanenbaum, Modern Operating Systems
