#include "../../include/kernel/heap.h"
#include "../../include/kernel/string.h"
#include "../../include/kernel/types.h"
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
  
  // Use slab allocator for allocations < 4KB
  if (g_slab_initialized && size < 4096) {
    slab_cache_t *cache = NULL;
    
    if (size <= 16) cache = g_cache_16;
    else if (size <= 32) cache = g_cache_32;
    else if (size <= 64) cache = g_cache_64;
    else if (size <= 128) cache = g_cache_128;
    else if (size <= 256) cache = g_cache_256;
    else if (size <= 512) cache = g_cache_512;
    else if (size <= 1024) cache = g_cache_1024;
    else if (size <= 2048) cache = g_cache_2048;
    
    if (cache) {
      void *ptr = slab_alloc(cache);
      if (ptr) {
        return ptr;
      }
      // Fall through to heap allocation if slab fails
    }
  }
  
  // Use heap for large allocations or if slab is not available
  size = align16(size);
  block_header_t *b = g_heap_head;
  while (b) {
    if (b->free && b->size >= size) {
      split_block(b, size);
      b->free = 0;
      return (uint8_t *)b + sizeof(block_header_t);
    }
    b = b->next;
  }
  return NULL;
}
void *kcalloc(size_t num, size_t size) {
  size_t total = 0;
  if (num && size) {
    total = num * size;
    if (total / num != size)
      return NULL;
  }
  
  // kmalloc now handles slab allocation internally
  void *p = kmalloc(total);
  if (p) {
    volatile uint8_t *q = p;
    for (size_t i = 0; i < total; i++)
      q[i] = 0;
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
  
  // Determine current allocation size
  size_t old_size = 0;
  
  if (is_heap_pointer(ptr)) {
    // Heap allocation
    size = align16(size);
    block_header_t *b =
        (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    old_size = b->size;
    if (old_size >= size)
      return ptr;
  } else if (g_slab_initialized) {
    // Slab allocation - need to find the cache
    slab_cache_t *cache = find_slab_cache(ptr, &old_size);
    if (cache && old_size >= size) {
      return ptr;
    }
  }
  
  // Need to allocate new memory
  void *n = kmalloc(size);
  if (!n)
    return NULL;
  
  // Copy old data
  volatile uint8_t *d = n;
  volatile uint8_t *s = ptr;
  size_t copy_size = (old_size > 0 && old_size < size) ? old_size : size;
  for (size_t i = 0; i < copy_size; i++)
    d[i] = s[i];
  
  kfree(ptr);
  return n;
}
void kfree(void *ptr) {
  if (!ptr)
    return;
  
  // Check if this is a slab-allocated object
  if (g_slab_initialized && !is_heap_pointer(ptr)) {
    size_t size = 0;
    slab_cache_t *cache = find_slab_cache(ptr, &size);
    if (cache) {
      slab_free(cache, ptr);
      return;
    }
  }
  
  // Otherwise, it's a heap allocation
  block_header_t *b =
      (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
  b->free = 1;
  coalesce(b);
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
