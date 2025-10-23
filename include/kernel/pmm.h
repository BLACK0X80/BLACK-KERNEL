#pragma once
#include "multiboot2.h"
#include "types.h"

void pmm_init(multiboot_mmap_entry_t *mmap, uint32_t mmap_size);
void pmm_mark_region_used(uint64_t base, uint64_t size);
void pmm_mark_region_free(uint64_t base, uint64_t size);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t frame);
uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_free_memory(void);
