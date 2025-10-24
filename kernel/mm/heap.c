#include "../../include/kernel/heap.h"
#include "../../include/kernel/config.h"
#include "../../include/kernel/string.h"
#include "../../include/kernel/types.h"
#include "../../include/kernel/stdio.h"
#include "../../include/mm/slab.h"

typedef struct block_header_t {
  size_t size;
  int free;
  struct block_header_t *next;
  struct block_header_t *prev;
} block_header_t;

static uint8_t *g_heap_start = 0;
static size_t g_heap_size = 0;
static block_header_t *g_heap_head = 0;
static int g_slab_initialized = 0;

// Slab caches for common allocation sizes
static slab_cache_t *g_cache_16 = 0;
static slab_cache_t *g_cache_32 = 0;
static slab_cache_t *g_cache_64 = 0;
static slab_cache_t *g_cache_128 = 0;
static slab_cache_t *g_cache_256 = 0;
static slab_cache_t *g_cache_512 = 0;
static slab_cache_t *g_cache_1024 = 0;
static slab_cache_t *g_cache_2048 = 0;

static size_t align16(size_t n) { return (n + 15) & ~(size_t)15; }

// Helper to determine if pointer is from heap
static int is_heap_pointer(void *ptr) {
  uintptr_t addr = (uintptr_t)ptr;
  uintptr_t heap_start = (uintptr_t)g_heap_start;
  uintptr_t heap_end = heap_start + g_heap_size;
  return (addr >= heap_start && addr < heap_end);
}

// Helper to find which slab cache a pointer belongs to
static slab_cache_t *find_slab_cache(void *ptr, size_t *out_size) {
  if (!g_slab_initialized || !ptr) {
    return NULL;
  }
  
  // Try to determine size by checking cache boundaries
  // This is a simplified approach - in production, you'd want better tracking
  slab_cache_t *caches[] = {
    g_cache_16, g_cache_32, g_cache_64, g_cache_128,
    g_cache_256, g_cache_512, g_cache_1024, g_cache_2048
  };
  size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
  
  for (int i = 0; i < 8; i++) {
    if (caches[i]) {
      // Simple heuristic: if not in heap range, assume it's from slab
      if (!is_heap_pointer(ptr)) {
        if (out_size) *out_size = sizes[i];
        return caches[i];
      }
    }
  }
  
  return NULL;
}
void heap_init(uint64_t start, uint64_t size) {
  g_heap_start = (uint8_t *)(uintptr_t)start;
  g_heap_size = (size_t)size;
  g_heap_head = (block_header_t *)g_heap_start;
  g_heap_head->size = g_heap_size - sizeof(block_header_t);
  g_heap_head->free = 1;
  g_heap_head->next = 0;
  g_heap_head->prev = 0;
  
  // Initialize slab caches for common allocation sizes
  // Note: slab_init() must be called before heap_init()
  if (g_slab_initialized) {
    g_cache_16 = slab_cache_create("kmalloc-16", 16, 16);
    g_cache_32 = slab_cache_create("kmalloc-32", 32, 16);
    g_cache_64 = slab_cache_create("kmalloc-64", 64, 16);
    g_cache_128 = slab_cache_create("kmalloc-128", 128, 16);
    g_cache_256 = slab_cache_create("kmalloc-256", 256, 16);
    g_cache_512 = slab_cache_create("kmalloc-512", 512, 16);
    g_cache_1024 = slab_cache_create("kmalloc-1024", 1024, 16);
    g_cache_2048 = slab_cache_create("kmalloc-2048", 2048, 16);
  }
}

void heap_enable_slab(void) {
  g_slab_initialized = 1;
}
static void split_block(block_header_t *b, size_t size) {
  if (b->size >= size + sizeof(block_header_t) + 16) {
    block_header_t *n =
        (block_header_t *)((uint8_t *)b + sizeof(block_header_t) + size);
    n->size = b->size - size - sizeof(block_header_t);
    n->free = 1;
    n->next = b->next;
    n->prev = b;
    if (n->next)
      n->next->prev = n;
    b->next = n;
    b->size = size;
  }
}
static void coalesce(block_header_t *b) {
  if (b->next && b->next->free) {
    b->size += sizeof(block_header_t) + b->next->size;
    b->next = b->next->next;
    if (b->next)
      b->next->prev = b;
  }
  if (b->prev && b->prev->free) {
    b = b->prev;
    if (b->next && b->next->free) {
      b->size += sizeof(block_header_t) + b->next->size;
      b->next = b->next->next;
      if (b->next)
        b->next->prev = b;
    }
  }
}
void *kmalloc(size_t size) {
  if (size == 0)
    return NULL;
  
  // Determine allocation source and cache index
  slab_cache_t *cache = NULL;
  uint8_t cache_index = SLAB_CACHE_NONE;
  uint16_t alloc_flags = ALLOC_FROM_HEAP;
  
  // Use slab allocator for allocations < 4KB
  if (g_slab_initialized && size < 4096) {
    if (size <= 16) { cache = g_cache_16; cache_index = SLAB_CACHE_16; }
    else if (size <= 32) { cache = g_cache_32; cache_index = SLAB_CACHE_32; }
    else if (size <= 64) { cache = g_cache_64; cache_index = SLAB_CACHE_64; }
    else if (size <= 128) { cache = g_cache_128; cache_index = SLAB_CACHE_128; }
    else if (size <= 256) { cache = g_cache_256; cache_index = SLAB_CACHE_256; }
    else if (size <= 512) { cache = g_cache_512; cache_index = SLAB_CACHE_512; }
    else if (size <= 1024) { cache = g_cache_1024; cache_index = SLAB_CACHE_1024; }
    else if (size <= 2048) { cache = g_cache_2048; cache_index = SLAB_CACHE_2048; }
    
    if (cache) {
      // Allocate from slab (includes space for header)
      void *slab_ptr = slab_alloc(cache);
      if (slab_ptr) {
        // Fill allocation header
        alloc_header_t *header = (alloc_header_t *)slab_ptr;
        header->magic = ALLOC_MAGIC;
        header->size = (uint32_t)size;
        header->flags = ALLOC_FROM_SLAB;
        header->slab_cache_index = cache_index;
        header->padding = 0;
        
        // Return pointer after header
        void *user_ptr = (uint8_t *)slab_ptr + sizeof(alloc_header_t);
        DEBUG_PRINT(SLAB, "kmalloc(%zu) from slab cache %u -> %p\n", 
                    size, cache_index, user_ptr);
        return user_ptr;
      }
      // Fall through to heap allocation if slab fails
      DEBUG_PRINT(SLAB, "Slab allocation failed for size %zu, falling back to heap\n", size);
    }
  }
  
  // Use heap for large allocations or if slab is not available
  // Need to allocate: header + requested size
  size_t total_size = sizeof(alloc_header_t) + size;
  total_size = align16(total_size);
  
  block_header_t *b = g_heap_head;
  while (b) {
    if (b->free && b->size >= total_size) {
      split_block(b, total_size);
      b->free = 0;
      
      // Fill allocation header
      alloc_header_t *header = (alloc_header_t *)((uint8_t *)b + sizeof(block_header_t));
      header->magic = ALLOC_MAGIC;
      header->size = (uint32_t)size;
      header->flags = ALLOC_FROM_HEAP;
      header->slab_cache_index = SLAB_CACHE_NONE;
      header->padding = 0;
      
      // Return pointer after header
      void *user_ptr = (uint8_t *)header + sizeof(alloc_header_t);
      DEBUG_PRINT(SLAB, "kmalloc(%zu) from heap -> %p\n", size, user_ptr);
      return user_ptr;
    }
    b = b->next;
  }
  
  kprintf("[HEAP] ERROR: kmalloc(%zu) failed - out of memory\n", size);
  return NULL;
}
void *kcalloc(size_t num, size_t size) {
  // Check for overflow
  size_t total = 0;
  if (num && size) {
    total = num * size;
    if (total / num != size) {
      kprintf("[HEAP] ERROR: kcalloc(%zu, %zu) overflow detected\n", num, size);
      return NULL;
    }
  }
  
  // kmalloc now handles slab allocation internally and adds headers
  void *p = kmalloc(total);
  if (p) {
    // Zero-fill the user data (not the header)
    volatile uint8_t *q = p;
    for (size_t i = 0; i < total; i++)
      q[i] = 0;
    DEBUG_PRINT(SLAB, "kcalloc(%zu, %zu) -> %p\n", num, size, p);
  }
  return p;
}
void *krealloc(void *ptr, size_t size) {
  if (!ptr)
    return kmalloc(size);
  if (size == 0) {
    kfree(ptr);
    return NULL;
  }
  
  // Get allocation header to determine current size
  alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
  
  // Validate magic number
  if (header->magic != ALLOC_MAGIC) {
    kprintf("[HEAP] ERROR: krealloc(%p, %zu) - invalid magic number 0x%x\n",
            ptr, size, header->magic);
    return NULL;
  }
  
  size_t old_size = header->size;
  
  // If new size fits in current allocation, just return same pointer
  if (size <= old_size) {
    DEBUG_PRINT(SLAB, "krealloc(%p, %zu) - reusing existing allocation (old size %zu)\n",
                ptr, size, old_size);
    return ptr;
  }
  
  // Need to allocate new memory
  void *n = kmalloc(size);
  if (!n) {
    kprintf("[HEAP] ERROR: krealloc(%p, %zu) - allocation failed\n", ptr, size);
    return NULL;
  }
  
  // Copy old data
  volatile uint8_t *d = n;
  volatile uint8_t *s = ptr;
  for (size_t i = 0; i < old_size; i++)
    d[i] = s[i];
  
  DEBUG_PRINT(SLAB, "krealloc(%p, %zu) - allocated new block %p, copied %zu bytes\n",
              ptr, size, n, old_size);
  
  kfree(ptr);
  return n;
}
void kfree(void *ptr) {
  if (!ptr) {
    DEBUG_PRINT(SLAB, "kfree(NULL) called - ignoring\n");
    return;
  }
  
  // Get allocation header
  alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
  
  // Validate magic number
  if (header->magic != ALLOC_MAGIC) {
    kprintf("[HEAP] ERROR: kfree(%p) - invalid magic number 0x%x (expected 0x%x)\n",
            ptr, header->magic, ALLOC_MAGIC);
    kprintf("[HEAP] ERROR: Possible corruption or double-free detected\n");
    return;
  }
  
  // Check allocation source from header
  if (header->flags & ALLOC_FROM_SLAB) {
    // Slab allocation - use cache index for O(1) lookup
    uint8_t cache_index = header->slab_cache_index;
    
    if (cache_index >= 8) {
      kprintf("[HEAP] ERROR: kfree(%p) - invalid slab cache index %u\n", 
              ptr, cache_index);
      return;
    }
    
    slab_cache_t *caches[] = {
      g_cache_16, g_cache_32, g_cache_64, g_cache_128,
      g_cache_256, g_cache_512, g_cache_1024, g_cache_2048
    };
    
    slab_cache_t *cache = caches[cache_index];
    if (!cache) {
      kprintf("[HEAP] ERROR: kfree(%p) - slab cache %u not initialized\n",
              ptr, cache_index);
      return;
    }
    
    // Free to slab cache (pass the header pointer, not user pointer)
    DEBUG_PRINT(SLAB, "kfree(%p) to slab cache %u (size %u)\n", 
                ptr, cache_index, header->size);
    slab_free(cache, header);
    return;
  }
  
  if (header->flags & ALLOC_FROM_HEAP) {
    // Heap allocation - get block header
    // Layout: [block_header_t][alloc_header_t][user data]
    block_header_t *b = (block_header_t *)((uint8_t *)header - sizeof(block_header_t));
    
    DEBUG_PRINT(SLAB, "kfree(%p) from heap (size %u)\n", ptr, header->size);
    
    b->free = 1;
    coalesce(b);
    return;
  }
  
  // Unknown allocation source
  kprintf("[HEAP] ERROR: kfree(%p) - unknown allocation source (flags 0x%x)\n",
          ptr, header->flags);
}

// Allocation with GFP flags support
void *kmalloc_flags(size_t size, uint32_t flags) {
  void *ptr = kmalloc(size);
  
  // Handle GFP_ZERO flag
  if (ptr && (flags & 0x04)) {  // GFP_ZERO
    volatile uint8_t *p = ptr;
    for (size_t i = 0; i < size; i++) {
      p[i] = 0;
    }
  }
  
  return ptr;
}

void *kcalloc_flags(size_t num, size_t size, uint32_t flags) {
  // kcalloc already zeros memory, so just call it
  return kcalloc(num, size);
}
