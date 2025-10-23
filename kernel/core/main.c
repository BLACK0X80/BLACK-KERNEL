#include "../../include/drivers/serial.h"
#include "../../include/drivers/vga.h"
#include "../../include/drivers/pic.h"
#include "../../include/drivers/pit.h"
#include "../../include/drivers/keyboard.h"
#include "../../include/kernel/gdt.h"
#include "../../include/kernel/heap.h"
#include "../../include/kernel/idt.h"
#include "../../include/kernel/interrupts.h"
#include "../../include/kernel/multiboot2.h"
#include "../../include/kernel/panic.h"
#include "../../include/kernel/pmm.h"
#include "../../include/kernel/stdio.h"
#include "../../include/kernel/types.h"
#include "../../include/kernel/vmm.h"
#include "../../include/mm/buddy.h"
#include "../../include/mm/slab.h"
#include "../../include/mm/cow.h"
#include "../../include/mm/demand_paging.h"
#include "../../include/mm/page_cache.h"
#include "../../include/kernel/test_runner.h"

void kernel_main(uint32_t multiboot_magic, void *multiboot_info) {
  vga_init();
  serial_init();
  kprintf("[PROMETHEUS] Booting kernel v1.0.0\n");
  kprintf("[PROMETHEUS] Initializing GDT... OK\n");
  gdt_init();
  gdt_load(0);
  kprintf("[PROMETHEUS] Initializing IDT... OK\n");
  interrupts_init();
  kprintf("[PROMETHEUS] Initializing PIC... OK\n");
  kprintf("[PROMETHEUS] Initializing PIT... OK\n");
  if (multiboot_magic == MULTIBOOT2_MAGIC) {
    uint8_t *tags = (uint8_t *)multiboot_info;
    multiboot_mmap_entry_t *mm = 0;
    uint32_t mm_size = 0;
    while (1) {
      multiboot_tag_t *t = (multiboot_tag_t *)tags;
      if (t->type == 0)
        break;
      if (t->type == 6) {
        multiboot_tag_mmap_t *mt = (multiboot_tag_mmap_t *)t;
        mm = (multiboot_mmap_entry_t *)((uint8_t *)mt +
                                        sizeof(multiboot_tag_mmap_t));
        mm_size = mt->size - sizeof(multiboot_tag_mmap_t);
      }
      tags += (t->size + 7) & ~7;
    }
    if (mm && mm_size) {
      // Initialize PMM (which now uses buddy allocator internally)
      pmm_init(mm, mm_size);
      kprintf("[PROMETHEUS] Initializing PMM/Buddy... OK\n");
    }
  }
  
  // Display memory statistics
  kprintf("[PROMETHEUS] Physical Memory: %u MB\n",
          (unsigned)(pmm_get_total_memory() / 1024 / 1024));
  kprintf("[PROMETHEUS] Free Memory: %u MB\n",
          (unsigned)(pmm_get_free_memory() / 1024 / 1024));
  
  // Initialize slab allocator (depends on buddy allocator)
  slab_init();
  kprintf("[PROMETHEUS] Initializing Slab... OK\n");
  
  // Initialize VMM
  vmm_init();
  kprintf("[PROMETHEUS] Initializing VMM... OK\n");
  
  // Initialize heap and enable slab integration
  heap_init(0x0000000080000000ULL, 16 * 1024 * 1024ULL);
  heap_enable_slab();
  kprintf("[PROMETHEUS] Initializing Heap... OK\n");
  
  // Initialize advanced memory features
  cow_init();
  kprintf("[PROMETHEUS] Initializing COW... OK\n");
  
  demand_paging_init();
  kprintf("[PROMETHEUS] Initializing Demand Paging... OK\n");
  
  // Initialize page cache with 1024 pages (4 MB)
  page_cache_init(1024);
  kprintf("[PROMETHEUS] Initializing Page Cache... OK\n");
  
  // Display updated memory statistics
  kprintf("[PROMETHEUS] Free Memory After Init: %u MB\n",
          (unsigned)(pmm_get_free_memory() / 1024 / 1024));
  keyboard_init();
  pic_unmask_irq(0);
  pic_unmask_irq(1);
  kprintf("[PROMETHEUS] Initializing Keyboard... OK\n");
  kprintf("[PROMETHEUS] Kernel ready.\n\n");
  
  // Run memory management tests
  run_all_memory_tests();
  
  kprintf("\n[PROMETHEUS] Tests complete. Awaiting input...\n");
  for (;;) {
    __asm__ volatile("hlt");
  }
}
