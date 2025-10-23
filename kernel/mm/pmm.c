#include "../../include/kernel/pmm.h"
#include "../../include/kernel/types.h"
#include "../../include/kernel/stdio.h"
#include "../../include/mm/buddy.h"

#define FRAME_SIZE 4096ULL

// Compatibility layer: PMM now uses buddy allocator as backend
static uint64_t g_pmm_total = 0;
static uint64_t g_pmm_free = 0;
static int g_buddy_initialized = 0;
void pmm_mark_region_used(uint64_t base, uint64_t size) {
  // This function is kept for compatibility but is now a no-op
  // The buddy allocator handles region management internally
  (void)base;
  (void)size;
}

void pmm_mark_region_free(uint64_t base, uint64_t size) {
  // This function is kept for compatibility but is now a no-op
  // The buddy allocator handles region management internally
  (void)base;
  (void)size;
}
void pmm_init(multiboot_mmap_entry_t *mmap, uint32_t mmap_size) {
  // Parse memory map to find the largest usable region
  uint64_t memory_start = 0;
  uint64_t memory_size = 0;
  
  g_pmm_total = 0;
  g_pmm_free = 0;
  
  uint8_t *p = (uint8_t *)mmap;
  uint8_t *end = p + mmap_size;
  
  while (p < end) {
    multiboot_mmap_entry_t *e = (multiboot_mmap_entry_t *)p;
    g_pmm_total += e->len;
    
    // Find largest usable memory region (type 1 = available)
    if (e->type == 1 && e->len > memory_size) {
      memory_start = e->addr;
      memory_size = e->len;
    }
    
    p += sizeof(multiboot_mmap_entry_t);
  }
  
  // Initialize buddy allocator with the largest usable region
  if (memory_size > 0) {
    buddy_init(memory_start, memory_size);
    g_buddy_initialized = 1;
    g_pmm_free = memory_size;
  }
}
uint64_t pmm_alloc_frame(void) {
  if (!g_buddy_initialized) {
    kprintf("[PMM] ERROR: Buddy allocator not initialized\n");
    return 0;
  }
  
  // Use buddy allocator to allocate a single page (order 0)
  uint64_t frame = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
  
  if (frame == 0) {
    kprintf("[PMM] ERROR: Failed to allocate frame\n");
    return 0;
  }
  
  if (g_pmm_free >= FRAME_SIZE) {
    g_pmm_free -= FRAME_SIZE;
  }
  
  return frame;
}

void pmm_free_frame(uint64_t frame) {
  if (!g_buddy_initialized) {
    kprintf("[PMM] ERROR: Buddy allocator not initialized in free\n");
    return;
  }
  
  if (frame == 0) {
    kprintf("[PMM] ERROR: Attempt to free NULL frame\n");
    return;
  }
  
  // Use buddy allocator to free a single page (order 0)
  buddy_free_pages(frame, 0);
  g_pmm_free += FRAME_SIZE;
}
uint64_t pmm_get_total_memory(void) { 
  return g_pmm_total; 
}

uint64_t pmm_get_free_memory(void) { 
  if (g_buddy_initialized) {
    return buddy_get_free_pages() * FRAME_SIZE;
  }
  return g_pmm_free;
}
