#pragma once
#include "../kernel/types.h"
#include "../kernel/spinlock.h"
#include "../kernel/vmm.h"

/**
 * Maximum number of concurrent address spaces (processes)
 * 
 * This limit is based on:
 * - Typical server workload: 100-200 concurrent processes
 * - Safety margin for burst scenarios
 * - Memory overhead: 256 * sizeof(vm_address_space_t) ≈ 8KB
 * 
 * Can be increased if needed, but consider:
 * - Memory overhead grows linearly with this value
 * - Linear search performance in demand_paging_get_address_space()
 * - For >256 processes, consider hash table instead of array
 */
#define MAX_ADDRESS_SPACES 256

// VM region flags
#define VM_FLAG_DEMAND_PAGED 0x01
#define VM_FLAG_ZERO_FILL 0x02
#define VM_FLAG_FILE_BACKED 0x04

// VM region structure for tracking virtual memory regions
typedef struct vm_region {
    uint64_t start;              // Region start address (page-aligned)
    uint64_t end;                // Region end address (page-aligned)
    uint32_t flags;              // VM_FLAG_* flags
    spinlock_t page_fault_lock;  // Synchronizes concurrent page fault handling
    struct vm_region *next;      // Next region in linked list
} vm_region_t;

// Address space structure containing region list
typedef struct vm_address_space {
    page_table_t *pml4;
    vm_region_t *regions;
    spinlock_t lock;
} vm_address_space_t;

// Demand paging subsystem functions
void demand_paging_init(void);
int demand_paging_register_region(page_table_t *pml4, uint64_t start, uint64_t size, uint32_t flags);
int demand_paging_handle_fault(page_table_t *pml4, uint64_t virt_addr);
void demand_paging_unregister_region(page_table_t *pml4, uint64_t start);

// Helper functions
vm_region_t *demand_paging_find_region(page_table_t *pml4, uint64_t virt_addr);
vm_address_space_t *demand_paging_get_address_space(page_table_t *pml4);
