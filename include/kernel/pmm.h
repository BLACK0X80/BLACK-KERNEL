#pragma once
#include "multiboot2.h"
#include "types.h"

/**
 * Physical Memory Manager (PMM)
 * 
 * The PMM is now a thin wrapper around the buddy allocator.
 * All physical memory management is handled internally by the buddy allocator.
 * 
 * Use buddy_alloc_pages() and buddy_free_pages() directly for more control
 * over zone selection and allocation order.
 */

void pmm_init(multiboot_mmap_entry_t *mmap, uint32_t mmap_size);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t frame);
uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_free_memory(void);
