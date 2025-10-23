#pragma once
#include "../kernel/types.h"
#include "../kernel/spinlock.h"
#include "../kernel/vmm.h"

// VM region flags
#define VM_FLAG_DEMAND_PAGED 0x01
#define VM_FLAG_ZERO_FILL 0x02
#define VM_FLAG_FILE_BACKED 0x04

// VM region structure for tracking virtual memory regions
typedef struct vm_region {
    uint64_t start;
    uint64_t end;
    uint32_t flags;
    struct vm_region *next;
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
