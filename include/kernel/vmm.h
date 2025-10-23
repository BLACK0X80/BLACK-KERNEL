#pragma once
#include "types.h"
#define VMM_FLAG_PRESENT 0x001
#define VMM_FLAG_WRITABLE 0x002
#define VMM_FLAG_USER 0x004
#define VMM_FLAG_NO_EXECUTE (1ULL << 63)
typedef uint64_t page_table_t;
void vmm_init(void);
page_table_t *vmm_create_address_space(void);
void vmm_switch_address_space(page_table_t *pml4);
void vmm_map_page(page_table_t *pml4, uint64_t virt, uint64_t phys,
                  uint32_t flags);
void vmm_unmap_page(page_table_t *pml4, uint64_t virt);
uint64_t vmm_get_physical_address(page_table_t *pml4, uint64_t virt);
