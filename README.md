Prometheus Kernel

Overview
Prometheus is a freestanding x86_64 kernel booted via Multiboot2. It brings up 64-bit long mode, initializes GDT/IDT, PIC/PIT, basic drivers, and provides PMM/VMM and a simple heap.

Features

- Multiboot2 32->64 bit transition
- Long mode with 2MiB early paging then 4-level paging API
- GDT/IDT with ISR/IRQ stubs and common handler
- PIC remap, PIT timer (1000Hz), PS/2 keyboard
- VGA text mode and COM1 serial I/O
- PMM bitmap allocator, VMM page mapping API
- Simple first-fit heap allocator

Build
Requirements: nasm, x86_64-elf-gcc, x86_64-elf-ld, grub-mkrescue, qemu

Windows: use WSL or a cross toolchain providing the above.

Commands:
make
./run.sh
./debug.sh

Run
make run will launch QEMU and print initialization logs to VGA and serial.

Debug
./debug.sh starts QEMU with a GDB server on tcp::1234. Connect using gdb target remote :1234.

Architecture
boot/ contains the Multiboot2 header and paging bring-up.
kernel/arch/x86_64/ contains low-level assembly for entry, GDT, IDT, and interrupt stubs.
kernel/core/ contains initialization, panic, and interrupt management.
kernel/mm/ contains PMM (bitmap), VMM (page tables), and heap.
kernel/drivers/ contains VGA, serial, PS/2 keyboard, PIT, and PIC drivers.
include/ exposes public headers.

Memory Map

- Kernel loaded at 1MiB physical, mapped to higher half 0xFFFFFFFF80000000.
- Heap starts at 0x0000000080000000 with 16MiB region.

Contributing
Use consistent naming and no comments in code. Submit PRs with clear commit messages and tested changes.

License
MIT


## Debug Configuration

Prometheus Kernel includes a comprehensive debug logging system that can be enabled or disabled at compile time for zero runtime overhead in production builds.

### Enabling/Disabling Debug Logging

Debug configuration is controlled in `include/kernel/config.h`:

```c
// Master debug switch
#define DEBUG_MODE 1  // Set to 0 to disable all debug logging

// Per-module debug flags
#define DEBUG_BUDDY 1             // Buddy allocator debug
#define DEBUG_SLAB 1              // Slab allocator debug
#define DEBUG_COW 1               // Copy-on-write debug
#define DEBUG_DEMAND_PAGING 1     // Demand paging debug
#define DEBUG_PAGE_CACHE 1        // Page cache debug
```

### Available Debug Modules

| Module | Flag | What It Logs |
|--------|------|--------------|
| **Buddy Allocator** | `DEBUG_BUDDY` | Zone selection decisions, invalid flag warnings, allocation/free operations, memory statistics |
| **Slab Allocator** | `DEBUG_SLAB` | Cache creation/destruction, allocation/free operations, slab creation failures, object not found warnings |
| **Copy-on-Write** | `DEBUG_COW` | Page marking operations, page fault handling, page copy operations, reference count changes |
| **Demand Paging** | `DEBUG_DEMAND_PAGING` | Region registration, page fault handling, race condition detection, lock acquisition/release |
| **Page Cache** | `DEBUG_PAGE_CACHE` | Cache hits/misses, page insertion/eviction, hash function validation, LRU operations |

### Debug Output Examples

**Buddy Allocator:**
```
[DEBUG:BUDDY] Selected MOVABLE zone for allocation (order 0)
[DEBUG:BUDDY] Zero-filled 4096 bytes at 0x100000
```

**Slab Allocator:**
```
[DEBUG:SLAB] kmalloc(128) from slab cache 3 -> 0x200000
[DEBUG:SLAB] kfree(0x200000) to slab cache 3 (size 128)
```

**Copy-on-Write:**
```
[DEBUG:COW] Handling COW fault at virt 0x400000, phys 0x100000, refcount 2
[DEBUG:COW] Multiple references (2), copying page from 0x100000
[DEBUG:COW] Copy started: old_phys=0x100000, new_phys=0x101000
[DEBUG:COW] Copy completed, updating PTE for virt 0x400000
```

**Demand Paging:**
```
[DEBUG:DEMAND_PAGING] Registered region [0x200000, 0x210000) with flags 0x3
[DEBUG:DEMAND_PAGING] Handling page fault at 0x201000
[DEBUG:DEMAND_PAGING] Zero-filled page at phys 0x102000
[DEBUG:DEMAND_PAGING] Mapped virt 0x201000 -> phys 0x102000
```

### Performance Impact

- **DEBUG_MODE=0**: Zero runtime overhead - all debug statements compile to no-ops
- **DEBUG_MODE=1**: Minimal overhead - only enabled modules generate output
- **Compile-time optimization**: Debug code is completely eliminated when disabled

### Usage in Code

Use the `DEBUG_PRINT` macro for debug logging:

```c
DEBUG_PRINT(BUDDY, "Allocated %u pages from zone %u\n", pages, zone);
```

This will only print if both `DEBUG_MODE` and `DEBUG_BUDDY` are enabled.



## Known Limitations

While Prometheus Kernel provides a solid foundation for memory management, there are several known limitations that may be addressed in future versions:

### Memory Management

**No OOM (Out-of-Memory) Killer**
- The kernel does not currently implement an OOM killer to handle memory exhaustion gracefully
- When memory is exhausted, allocations simply fail and return NULL
- Applications must handle allocation failures appropriately
- Future enhancement: Implement a policy-based OOM killer that can terminate low-priority processes to free memory

**No Memory Defragmentation**
- The buddy allocator does not perform active defragmentation
- Over time, memory can become fragmented, making large contiguous allocations difficult
- The zone-based allocation (MOVABLE, RECLAIMABLE, UNMOVABLE) lays groundwork for future defragmentation
- Future enhancement: Implement memory compaction for the MOVABLE zone to reduce fragmentation

**Single-Core Optimization**
- Current memory allocators are optimized for single-core systems
- While locks are used for correctness, there is no per-CPU caching or NUMA awareness
- Multi-core systems may experience lock contention under heavy allocation workloads
- Future enhancement: Implement per-CPU slab caches and NUMA-aware allocation policies

### Concurrency

**Limited Parallelism**
- Fine-grained locking is used (per-region, per-cache), but not optimized for high-core-count systems
- No lock-free data structures are currently employed
- Future enhancement: Implement lock-free allocation paths for common cases

### Resource Limits

**Fixed Address Space Limit**
- Maximum of 256 concurrent address spaces (processes) - see `MAX_ADDRESS_SPACES` in `include/mm/demand_paging.h`
- This is sufficient for typical workloads but may be limiting for container-heavy scenarios
- Future enhancement: Dynamic address space management with hash table instead of fixed array

**Fixed Hash Table Sizes**
- COW hash table: 1024 buckets (fixed)
- Page cache hash table: 1024 buckets (fixed)
- These sizes are reasonable for most workloads but not dynamically adjustable
- Future enhancement: Implement resizable hash tables that grow/shrink based on load

### Testing

**Limited Multi-Threading Tests**
- Current test suite simulates concurrent operations but doesn't use actual threads
- Real multi-core race conditions may not be fully exercised
- Future enhancement: Implement multi-threaded stress tests when threading support is added

### Performance

**No Memory Accounting**
- The kernel does not track per-process memory usage
- No memory limits or quotas can be enforced
- Future enhancement: Implement memory cgroups or similar accounting mechanism

**No Slab Reclaim**
- Empty slabs are not automatically freed back to the buddy allocator
- Memory used by slab caches remains allocated even when unused
- Future enhancement: Implement slab shrinker that frees empty slabs under memory pressure

### Compatibility

**x86-64 Only**
- Memory management code is currently x86-64 specific
- Page size is hardcoded to 4KB
- Future enhancement: Abstract architecture-specific details for portability

## Future Roadmap

Planned enhancements to address these limitations:

1. **Phase 1**: OOM killer and basic memory accounting
2. **Phase 2**: Memory defragmentation for MOVABLE zone
3. **Phase 3**: Per-CPU slab caches and NUMA awareness
4. **Phase 4**: Lock-free allocation paths
5. **Phase 5**: Dynamic resource limits and resizable hash tables

Contributions addressing any of these limitations are welcome!

