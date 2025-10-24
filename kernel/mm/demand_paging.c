#include "../../include/mm/demand_paging.h"
#include "../../include/mm/buddy.h"
#include "../../include/mm/slab.h"
#include "../../include/kernel/config.h"
#include "../../include/kernel/string.h"
#include "../../include/kernel/stdio.h"

// Global address space list
#define MAX_ADDRESS_SPACES 256
static vm_address_space_t address_spaces[MAX_ADDRESS_SPACES];
static uint32_t address_space_count = 0;
static spinlock_t global_lock;

// Slab cache for VM regions
static slab_cache_t *vm_region_cache = NULL;

void demand_paging_init(void) {
    // Initialize global lock
    spinlock_init(&global_lock);
    
    // Initialize address space array
    for (uint32_t i = 0; i < MAX_ADDRESS_SPACES; i++) {
        address_spaces[i].pml4 = NULL;
        address_spaces[i].regions = NULL;
        spinlock_init(&address_spaces[i].lock);
    }
    
    address_space_count = 0;
    
    // Create slab cache for VM regions
    vm_region_cache = slab_cache_create("vm_region", sizeof(vm_region_t), 8);
}

// Helper function to get or create address space for a page table
vm_address_space_t *demand_paging_get_address_space(page_table_t *pml4) {
    spinlock_acquire(&global_lock);
    
    // Search for existing address space
    for (uint32_t i = 0; i < address_space_count; i++) {
        if (address_spaces[i].pml4 == pml4) {
            spinlock_release(&global_lock);
            return &address_spaces[i];
        }
    }
    
    // Create new address space if not found
    if (address_space_count < MAX_ADDRESS_SPACES) {
        vm_address_space_t *as = &address_spaces[address_space_count];
        as->pml4 = pml4;
        as->regions = NULL;
        address_space_count++;
        spinlock_release(&global_lock);
        return as;
    }
    
    spinlock_release(&global_lock);
    return NULL;
}

// Helper function to find a region containing the given virtual address
vm_region_t *demand_paging_find_region(page_table_t *pml4, uint64_t virt_addr) {
    vm_address_space_t *as = demand_paging_get_address_space(pml4);
    if (!as) {
        return NULL;
    }
    
    spinlock_acquire(&as->lock);
    
    vm_region_t *region = as->regions;
    while (region) {
        if (virt_addr >= region->start && virt_addr < region->end) {
            spinlock_release(&as->lock);
            return region;
        }
        region = region->next;
    }
    
    spinlock_release(&as->lock);
    return NULL;
}

// Register a virtual memory region for demand paging
int demand_paging_register_region(page_table_t *pml4, uint64_t start, uint64_t size, uint32_t flags) {
    // Validate parameters
    if (!pml4 || size == 0) {
        return -1;
    }
    
    // Align start and size to page boundaries
    uint64_t aligned_start = start & ~(BUDDY_PAGE_SIZE - 1);
    uint64_t aligned_end = (start + size + BUDDY_PAGE_SIZE - 1) & ~(BUDDY_PAGE_SIZE - 1);
    
    // Get or create address space
    vm_address_space_t *as = demand_paging_get_address_space(pml4);
    if (!as) {
        return -1;
    }
    
    spinlock_acquire(&as->lock);
    
    // Check for overlapping regions
    vm_region_t *existing = as->regions;
    while (existing) {
        if ((aligned_start >= existing->start && aligned_start < existing->end) ||
            (aligned_end > existing->start && aligned_end <= existing->end) ||
            (aligned_start <= existing->start && aligned_end >= existing->end)) {
            spinlock_release(&as->lock);
            return -1;  // Overlap detected
        }
        existing = existing->next;
    }
    
    // Allocate new region
    vm_region_t *region = (vm_region_t *)slab_alloc(vm_region_cache);
    if (!region) {
        spinlock_release(&as->lock);
        return -1;
    }
    
    // Initialize region
    region->start = aligned_start;
    region->end = aligned_end;
    region->flags = flags;
    spinlock_init(&region->page_fault_lock);  // Initialize per-region lock
    region->next = as->regions;
    as->regions = region;
    
    DEBUG_PRINT(DEMAND_PAGING, "Registered region [0x%llx, 0x%llx) with flags 0x%x\n",
                aligned_start, aligned_end, flags);
    
    spinlock_release(&as->lock);
    return 0;
}

// Handle a page fault for demand paging
int demand_paging_handle_fault(page_table_t *pml4, uint64_t virt_addr) {
    // Align address to page boundary
    uint64_t aligned_addr = virt_addr & ~(BUDDY_PAGE_SIZE - 1);
    
    // Find the region containing this address
    vm_region_t *region = demand_paging_find_region(pml4, aligned_addr);
    if (!region) {
        DEBUG_PRINT(DEMAND_PAGING, "Address 0x%llx not in any registered region\n", virt_addr);
        return -1;  // Address not in any registered region
    }
    
    // Check if region supports demand paging
    if (!(region->flags & VM_FLAG_DEMAND_PAGED)) {
        DEBUG_PRINT(DEMAND_PAGING, "Region [0x%llx, 0x%llx) does not support demand paging\n",
                    region->start, region->end);
        return -1;
    }
    
    // First check (unlocked, fast path) - avoid lock if page already mapped
    uint64_t existing_phys = vmm_get_physical_address(pml4, aligned_addr);
    if (existing_phys != 0) {
        DEBUG_PRINT(DEMAND_PAGING, "Page already mapped at 0x%llx (fast path)\n", aligned_addr);
        return 0;  // Page already mapped by another thread
    }
    
    // Acquire per-region lock for synchronized page fault handling
    spinlock_acquire(&region->page_fault_lock);
    
    // Second check (locked) - prevent race condition
    existing_phys = vmm_get_physical_address(pml4, aligned_addr);
    if (existing_phys != 0) {
        // Another thread mapped the page while we were waiting for the lock
        spinlock_release(&region->page_fault_lock);
        DEBUG_PRINT(DEMAND_PAGING, "Page already mapped at 0x%llx (race detected)\n", aligned_addr);
        return 0;
    }
    
    DEBUG_PRINT(DEMAND_PAGING, "Handling page fault at 0x%llx\n", aligned_addr);
    
    // Allocate a physical page
    uint64_t phys_addr = buddy_alloc_pages(0, BUDDY_ZONE_MOVABLE);
    if (phys_addr == 0) {
        spinlock_release(&region->page_fault_lock);
        kprintf("[DEMAND_PAGING] ERROR: Out of memory for page fault at 0x%llx\n", aligned_addr);
        return -1;  // Out of memory
    }
    
    // Zero-fill the page if requested
    if (region->flags & VM_FLAG_ZERO_FILL) {
        // Map temporarily to zero it (assuming direct physical mapping at 0xFFFF800000000000)
        uint64_t direct_map_addr = 0xFFFF800000000000ULL + phys_addr;
        uint8_t *page_ptr = (uint8_t *)direct_map_addr;
        
        for (uint64_t i = 0; i < BUDDY_PAGE_SIZE; i++) {
            page_ptr[i] = 0;
        }
        DEBUG_PRINT(DEMAND_PAGING, "Zero-filled page at phys 0x%llx\n", phys_addr);
    }
    
    // Map the page in the page table
    uint32_t flags = VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER;
    vmm_map_page(pml4, aligned_addr, phys_addr, flags);
    
    DEBUG_PRINT(DEMAND_PAGING, "Mapped virt 0x%llx -> phys 0x%llx\n", aligned_addr, phys_addr);
    
    spinlock_release(&region->page_fault_lock);
    return 0;
}

// Unregister a virtual memory region
void demand_paging_unregister_region(page_table_t *pml4, uint64_t start) {
    // Align start to page boundary
    uint64_t aligned_start = start & ~(BUDDY_PAGE_SIZE - 1);
    
    // Get address space
    vm_address_space_t *as = demand_paging_get_address_space(pml4);
    if (!as) {
        return;
    }
    
    spinlock_acquire(&as->lock);
    
    // Find and remove the region
    vm_region_t *prev = NULL;
    vm_region_t *current = as->regions;
    
    while (current) {
        if (current->start == aligned_start) {
            // Remove from list
            if (prev) {
                prev->next = current->next;
            } else {
                as->regions = current->next;
            }
            
            // Optionally free allocated pages
            for (uint64_t addr = current->start; addr < current->end; addr += BUDDY_PAGE_SIZE) {
                uint64_t phys_addr = vmm_get_physical_address(pml4, addr);
                if (phys_addr != 0) {
                    vmm_unmap_page(pml4, addr);
                    buddy_free_pages(phys_addr, 0);
                }
            }
            
            // Free the region structure
            slab_free(vm_region_cache, current);
            
            spinlock_release(&as->lock);
            return;
        }
        
        prev = current;
        current = current->next;
    }
    
    spinlock_release(&as->lock);
}
