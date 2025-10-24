#include "../../include/mm/cow.h"
#include "../../include/mm/buddy.h"
#include "../../include/kernel/config.h"
#include "../../include/kernel/string.h"
#include "../../include/kernel/stdio.h"

// Global hash table for page reference tracking
static page_ref_t *cow_hash_table[COW_HASH_SIZE];
static spinlock_t cow_global_lock;

// Hash function for physical addresses
static uint32_t cow_hash(uint64_t phys_addr) {
    // Align to page boundary and hash
    phys_addr &= ~0xFFFULL;
    // Use bitwise AND instead of modulo for performance (20-40x faster)
    // This works because COW_HASH_SIZE is a power of 2
    return (uint32_t)((phys_addr >> 12) & COW_HASH_MASK);
}

// Initialize COW subsystem
void cow_init(void) {
    // Initialize hash table to NULL
    for (uint32_t i = 0; i < COW_HASH_SIZE; i++) {
        cow_hash_table[i] = 0;
    }
    
    // Initialize global lock
    spinlock_init(&cow_global_lock);
}

// Helper function to get page table entry pointer
static uint64_t *cow_get_pte(page_table_t *pml4, uint64_t virt_addr) {
    uint64_t *pml4t = (uint64_t *)(uintptr_t)pml4;
    
    // PML4 index
    uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t pml4_entry = pml4t[pml4_idx];
    if (!(pml4_entry & VMM_FLAG_PRESENT)) {
        return 0;
    }
    
    // PDPT index
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4_entry & 0x000FFFFFFFFFF000ULL);
    uint64_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pdpt_entry = pdpt[pdpt_idx];
    if (!(pdpt_entry & VMM_FLAG_PRESENT)) {
        return 0;
    }
    
    // PD index
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt_entry & 0x000FFFFFFFFFF000ULL);
    uint64_t pd_idx = (virt_addr >> 21) & 0x1FF;
    uint64_t pd_entry = pd[pd_idx];
    if (!(pd_entry & VMM_FLAG_PRESENT)) {
        return 0;
    }
    
    // PT index
    uint64_t *pt = (uint64_t *)(uintptr_t)(pd_entry & 0x000FFFFFFFFFF000ULL);
    uint64_t pt_idx = (virt_addr >> 12) & 0x1FF;
    
    return &pt[pt_idx];
}

// Get or create page reference entry
page_ref_t *cow_get_ref(uint64_t phys_addr) {
    phys_addr &= ~0xFFFULL;  // Align to page boundary
    uint32_t hash = cow_hash(phys_addr);
    
    spinlock_acquire(&cow_global_lock);
    
    // Search for existing entry
    page_ref_t *ref = cow_hash_table[hash];
    while (ref) {
        if (ref->physical_address == phys_addr) {
            spinlock_release(&cow_global_lock);
            return ref;
        }
        ref = ref->next;
    }
    
    // Create new entry
    ref = (page_ref_t *)buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    if (!ref) {
        spinlock_release(&cow_global_lock);
        DEBUG_PRINT(COW, "Failed to allocate reference entry for phys 0x%llx\n", phys_addr);
        return 0;
    }
    
    ref->physical_address = phys_addr;
    ref->ref_count = 0;
    spinlock_init(&ref->lock);
    ref->next = cow_hash_table[hash];
    cow_hash_table[hash] = ref;
    
    spinlock_release(&cow_global_lock);
    return ref;
}

// Mark a page as copy-on-write
int cow_mark_page(page_table_t *pml4, uint64_t virt_addr) {
    // Validate parameters
    if (!pml4) {
        DEBUG_PRINT(COW, "NULL pml4 in cow_mark_page for virt 0x%llx\n", virt_addr);
        return -1;
    }
    
    // Get page table entry
    uint64_t *pte = cow_get_pte(pml4, virt_addr);
    if (!pte) {
        DEBUG_PRINT(COW, "Page not mapped at 0x%llx\n", virt_addr);
        return -1;
    }
    
    uint64_t entry = *pte;
    if (!(entry & VMM_FLAG_PRESENT)) {
        DEBUG_PRINT(COW, "Page not present at 0x%llx\n", virt_addr);
        return -1;
    }
    
    // Get physical address
    uint64_t phys_addr = entry & 0x000FFFFFFFFFF000ULL;
    
    // Get or create reference entry
    page_ref_t *ref = cow_get_ref(phys_addr);
    if (!ref) {
        DEBUG_PRINT(COW, "Failed to allocate reference entry for phys 0x%llx (virt 0x%llx)\n", 
                    phys_addr, virt_addr);
        return -1;
    }
    
    // Increment reference count
    spinlock_acquire(&ref->lock);
    ref->ref_count++;
    spinlock_release(&ref->lock);
    
    // Set page read-only and set COW bit
    entry &= ~VMM_FLAG_WRITABLE;  // Clear writable flag
    entry |= COW_FLAG_MASK;        // Set COW flag
    *pte = entry;
    
    // Flush TLB for this address
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
    
    return 0;
}

// Handle copy-on-write page fault
int cow_handle_fault(page_table_t *pml4, uint64_t virt_addr) {
    // Get page table entry
    uint64_t *pte = cow_get_pte(pml4, virt_addr);
    if (!pte) {
        DEBUG_PRINT(COW, "Page not mapped at 0x%llx in cow_handle_fault\n", virt_addr);
        return -1;  // Page not mapped
    }
    
    uint64_t entry = *pte;
    if (!(entry & VMM_FLAG_PRESENT)) {
        DEBUG_PRINT(COW, "Page not present at 0x%llx in cow_handle_fault\n", virt_addr);
        return -1;  // Page not present
    }
    
    // Check if this is a COW page
    if (!(entry & COW_FLAG_MASK)) {
        DEBUG_PRINT(COW, "Not a COW page at 0x%llx\n", virt_addr);
        return -1;  // Not a COW page
    }
    
    // Get old physical address
    uint64_t old_phys = entry & 0x000FFFFFFFFFF000ULL;
    
    // Get reference entry
    page_ref_t *ref = cow_get_ref(old_phys);
    if (!ref) {
        DEBUG_PRINT(COW, "Reference entry not found for phys 0x%llx (virt 0x%llx)\n", 
                    old_phys, virt_addr);
        return -1;  // Reference entry not found
    }
    
    // Check reference count
    spinlock_acquire(&ref->lock);
    uint32_t ref_count = ref->ref_count;
    
    DEBUG_PRINT(COW, "Handling COW fault at virt 0x%llx, phys 0x%llx, refcount %u\n",
                virt_addr, old_phys, ref_count);
    
    // If we're the only reference, just make it writable
    if (ref_count == 1) {
        ref->ref_count--;
        spinlock_release(&ref->lock);
        
        DEBUG_PRINT(COW, "Single reference, making page writable at 0x%llx\n", virt_addr);
        
        // Make page writable and clear COW flag
        entry |= VMM_FLAG_WRITABLE;
        entry &= ~COW_FLAG_MASK;
        *pte = entry;
        
        // Flush TLB
        __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
        
        return 0;
    }
    
    // Multiple references - need to copy
    ref->ref_count--;
    spinlock_release(&ref->lock);
    
    DEBUG_PRINT(COW, "Multiple references (%u), copying page from 0x%llx\n", 
                ref_count, old_phys);
    
    // Allocate new physical page
    uint64_t new_phys = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    if (!new_phys) {
        // Restore reference count on failure
        spinlock_acquire(&ref->lock);
        ref->ref_count++;
        spinlock_release(&ref->lock);
        DEBUG_PRINT(COW, "Failed to allocate new page for COW at 0x%llx\n", virt_addr);
        return -1;  // Out of memory
    }
    
    DEBUG_PRINT(COW, "Copy started: old_phys=0x%llx, new_phys=0x%llx\n", old_phys, new_phys);
    
    // Copy page contents
    uint8_t *old_page = (uint8_t *)(uintptr_t)old_phys;
    uint8_t *new_page = (uint8_t *)(uintptr_t)new_phys;
    for (uint32_t i = 0; i < 4096; i++) {
        new_page[i] = old_page[i];
    }
    
    DEBUG_PRINT(COW, "Copy completed, updating PTE for virt 0x%llx\n", virt_addr);
    
    // Update page table entry
    entry = (new_phys & 0x000FFFFFFFFFF000ULL) | (entry & 0xFFF);
    entry |= VMM_FLAG_WRITABLE;  // Make writable
    entry &= ~COW_FLAG_MASK;      // Clear COW flag
    *pte = entry;
    
    // Flush TLB
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
    
    DEBUG_PRINT(COW, "COW fault handled successfully for virt 0x%llx\n", virt_addr);
    
    return 0;
}

// Increment reference count for a physical page
void cow_increment_ref(uint64_t phys_addr) {
    page_ref_t *ref = cow_get_ref(phys_addr);
    if (!ref) {
        return;  // Failed to get/create reference entry
    }
    
    spinlock_acquire(&ref->lock);
    ref->ref_count++;
    spinlock_release(&ref->lock);
}

// Decrement reference count and free page if count reaches zero
void cow_decrement_ref(uint64_t phys_addr) {
    phys_addr &= ~0xFFFULL;  // Align to page boundary
    uint32_t hash = cow_hash(phys_addr);
    
    spinlock_acquire(&cow_global_lock);
    
    // Find reference entry
    page_ref_t *ref = cow_hash_table[hash];
    page_ref_t *prev = 0;
    
    while (ref) {
        if (ref->physical_address == phys_addr) {
            break;
        }
        prev = ref;
        ref = ref->next;
    }
    
    if (!ref) {
        spinlock_release(&cow_global_lock);
        return;  // Reference entry not found
    }
    
    // Decrement reference count
    spinlock_acquire(&ref->lock);
    ref->ref_count--;
    uint32_t count = ref->ref_count;
    spinlock_release(&ref->lock);
    
    // If count reaches zero, free the physical page and reference entry
    if (count == 0) {
        // Remove from hash table
        if (prev) {
            prev->next = ref->next;
        } else {
            cow_hash_table[hash] = ref->next;
        }
        
        spinlock_release(&cow_global_lock);
        
        // Free physical page
        buddy_free_pages(phys_addr, 0);
        
        // Free reference entry
        buddy_free_pages((uint64_t)(uintptr_t)ref, 0);
    } else {
        spinlock_release(&cow_global_lock);
    }
}

// Get reference count for a physical page
uint32_t cow_get_ref_count(uint64_t phys_addr) {
    phys_addr &= ~0xFFFULL;  // Align to page boundary
    uint32_t hash = cow_hash(phys_addr);
    
    spinlock_acquire(&cow_global_lock);
    
    // Find reference entry
    page_ref_t *ref = cow_hash_table[hash];
    while (ref) {
        if (ref->physical_address == phys_addr) {
            spinlock_acquire(&ref->lock);
            uint32_t count = ref->ref_count;
            spinlock_release(&ref->lock);
            spinlock_release(&cow_global_lock);
            return count;
        }
        ref = ref->next;
    }
    
    spinlock_release(&cow_global_lock);
    return 0;  // Not found
}
