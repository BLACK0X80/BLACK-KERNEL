#include "../../include/kernel/vmm.h"
#include "../../include/kernel/pmm.h"
#include "../../include/kernel/types.h"
#define PAGE_SIZE 4096ULL
#define PT_ENTRIES 512
#define ADDR_PML4_INDEX(x) (((x) >> 39) & 0x1FF)
#define ADDR_PDPT_INDEX(x) (((x) >> 30) & 0x1FF)
#define ADDR_PD_INDEX(x) (((x) >> 21) & 0x1FF)
#define ADDR_PT_INDEX(x) (((x) >> 12) & 0x1FF)
static inline uint64_t *virt_to_ptr(uint64_t phys) {
  return (uint64_t *)(uintptr_t)phys;
}
static uint64_t *get_or_alloc(uint64_t *table, uint64_t index) {
  uint64_t e = table[index];
  if (!(e & VMM_FLAG_PRESENT)) {
    uint64_t frame = pmm_alloc_frame();
    if (frame == 0)
      return 0;
    for (size_t i = 0; i < PAGE_SIZE / 8; i++)
      virt_to_ptr(frame)[i] = 0;
    table[index] = frame | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE;
    e = table[index];
  }
  return virt_to_ptr(e & 0x000FFFFFFFFFF000ULL);
}
void vmm_init(void) {}
page_table_t *vmm_create_address_space(void) {
  uint64_t frame = pmm_alloc_frame();
  if (frame == 0)
    return 0;
  for (size_t i = 0; i < PAGE_SIZE / 8; i++)
    virt_to_ptr(frame)[i] = 0;
  return (page_table_t *)(uintptr_t)frame;
}
void vmm_switch_address_space(page_table_t *pml4) { (void)pml4; }
void vmm_map_page(page_table_t *pml4, uint64_t virt, uint64_t phys,
                  uint32_t flags) {
  uint64_t *pml4t = virt_to_ptr((uint64_t)(uintptr_t)pml4);
  uint64_t *pdpt = get_or_alloc(pml4t, ADDR_PML4_INDEX(virt));
  if (!pdpt)
    return;
  uint64_t *pd = get_or_alloc(pdpt, ADDR_PDPT_INDEX(virt));
  if (!pd)
    return;
  uint64_t *pt = get_or_alloc(pd, ADDR_PD_INDEX(virt));
  if (!pt)
    return;
  pt[ADDR_PT_INDEX(virt)] =
      (phys & 0x000FFFFFFFFFF000ULL) | (flags & 0xFFF) | VMM_FLAG_PRESENT;
}
void vmm_unmap_page(page_table_t *pml4, uint64_t virt) {
  uint64_t *pml4t = virt_to_ptr((uint64_t)(uintptr_t)pml4);
  uint64_t e = pml4t[ADDR_PML4_INDEX(virt)];
  if (!(e & VMM_FLAG_PRESENT))
    return;
  uint64_t *pdpt = virt_to_ptr(e & 0x000FFFFFFFFFF000ULL);
  e = pdpt[ADDR_PDPT_INDEX(virt)];
  if (!(e & VMM_FLAG_PRESENT))
    return;
  uint64_t *pd = virt_to_ptr(e & 0x000FFFFFFFFFF000ULL);
  e = pd[ADDR_PD_INDEX(virt)];
  if (!(e & VMM_FLAG_PRESENT))
    return;
  uint64_t *pt = virt_to_ptr(e & 0x000FFFFFFFFFF000ULL);
  pt[ADDR_PT_INDEX(virt)] = 0;
}
uint64_t vmm_get_physical_address(page_table_t *pml4, uint64_t virt) {
  uint64_t *pml4t = virt_to_ptr((uint64_t)(uintptr_t)pml4);
  uint64_t e = pml4t[ADDR_PML4_INDEX(virt)];
  if (!(e & VMM_FLAG_PRESENT))
    return 0;
  uint64_t *pdpt = virt_to_ptr(e & 0x000FFFFFFFFFF000ULL);
  e = pdpt[ADDR_PDPT_INDEX(virt)];
  if (!(e & VMM_FLAG_PRESENT))
    return 0;
  uint64_t *pd = virt_to_ptr(e & 0x000FFFFFFFFFF000ULL);
  e = pd[ADDR_PD_INDEX(virt)];
  if (!(e & VMM_FLAG_PRESENT))
    return 0;
  uint64_t *pt = virt_to_ptr(e & 0x000FFFFFFFFFF000ULL);
  e = pt[ADDR_PT_INDEX(virt)];
  if (!(e & VMM_FLAG_PRESENT))
    return 0;
  return (e & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}
