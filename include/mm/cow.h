#pragma once
#include "../kernel/types.h"
#include "../kernel/spinlock.h"
#include "../kernel/vmm.h"

// COW flag bit in page table entry (using available bit 9)
#define COW_FLAG_MASK 0x200

// Hash table size for page reference tracking
#define COW_HASH_SIZE 1024

// Page reference structure for tracking shared physical pages
typedef struct page_ref {
    uint64_t physical_address;
    uint32_t ref_count;
    spinlock_t lock;
    struct page_ref *next;  // For hash table chaining
} page_ref_t;

// COW subsystem functions
void cow_init(void);
int cow_mark_page(page_table_t *pml4, uint64_t virt_addr);
int cow_handle_fault(page_table_t *pml4, uint64_t virt_addr);
void cow_increment_ref(uint64_t phys_addr);
void cow_decrement_ref(uint64_t phys_addr);

// Helper functions
page_ref_t *cow_get_ref(uint64_t phys_addr);
uint32_t cow_get_ref_count(uint64_t phys_addr);
