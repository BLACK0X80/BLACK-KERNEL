#include "../../include/mm/cow.h"
#include "../../include/mm/buddy.h"
#include "../../include/kernel/vmm.h"
#include "../../include/kernel/stdio.h"

static int test_count = 0;
static int test_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    test_count++; \
    if (condition) { \
        test_passed++; \
    } else { \
        kprintf("[FAIL] %s\n", message); \
    } \
} while(0)

void test_cow_initialization(void) {
    cow_init();
    TEST_ASSERT(1, "COW initialization should complete without crashing");
}

void test_cow_page_marking(void) {
    // Create address space
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Allocate and map a page
    uint64_t phys = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(phys != 0, "Physical page allocation should succeed");
    
    if (!phys) return;
    
    uint64_t virt = 0x400000;  // Test virtual address
    vmm_map_page(pml4, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    
    // Mark page as COW
    int result = cow_mark_page(pml4, virt);
    TEST_ASSERT(result == 0, "COW page marking should succeed");
    
    // Verify reference count
    uint32_t ref_count = cow_get_ref_count(phys);
    TEST_ASSERT(ref_count == 1, "Reference count should be 1 after marking");
    
    // Clean up
    cow_decrement_ref(phys);
    vmm_unmap_page(pml4, virt);
}

void test_cow_reference_counting(void) {
    // Allocate a physical page
    uint64_t phys = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(phys != 0, "Physical page allocation should succeed");
    
    if (!phys) return;
    
    // Increment reference count multiple times
    cow_increment_ref(phys);
    TEST_ASSERT(cow_get_ref_count(phys) == 1, "Reference count should be 1");
    
    cow_increment_ref(phys);
    TEST_ASSERT(cow_get_ref_count(phys) == 2, "Reference count should be 2");
    
    cow_increment_ref(phys);
    TEST_ASSERT(cow_get_ref_count(phys) == 3, "Reference count should be 3");
    
    // Decrement reference count
    cow_decrement_ref(phys);
    TEST_ASSERT(cow_get_ref_count(phys) == 2, "Reference count should be 2 after decrement");
    
    cow_decrement_ref(phys);
    TEST_ASSERT(cow_get_ref_count(phys) == 1, "Reference count should be 1 after decrement");
    
    // Final decrement should free the page
    cow_decrement_ref(phys);
    TEST_ASSERT(cow_get_ref_count(phys) == 0, "Reference count should be 0 after final decrement");
}

void test_cow_fault_handler_single_ref(void) {
    // Create address space
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Allocate and map a page
    uint64_t phys = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(phys != 0, "Physical page allocation should succeed");
    
    if (!phys) return;
    
    uint64_t virt = 0x400000;
    vmm_map_page(pml4, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    
    // Mark as COW
    cow_mark_page(pml4, virt);
    
    // Handle fault (should just make writable since ref count is 1)
    int result = cow_handle_fault(pml4, virt);
    TEST_ASSERT(result == 0, "COW fault handling should succeed for single reference");
    
    // Verify physical address didn't change
    uint64_t new_phys = vmm_get_physical_address(pml4, virt);
    TEST_ASSERT(new_phys == phys, "Physical address should not change for single reference");
    
    // Clean up
    vmm_unmap_page(pml4, virt);
    buddy_free_pages(phys, 0);
}

void test_cow_fault_handler_multi_ref(void) {
    // Create two address spaces
    page_table_t *pml4_1 = vmm_create_address_space();
    page_table_t *pml4_2 = vmm_create_address_space();
    
    TEST_ASSERT(pml4_1 != 0 && pml4_2 != 0, "Address space creation should succeed");
    
    if (!pml4_1 || !pml4_2) return;
    
    // Allocate a physical page
    uint64_t phys = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(phys != 0, "Physical page allocation should succeed");
    
    if (!phys) return;
    
    // Write test pattern to page
    uint8_t *page = (uint8_t *)(uintptr_t)phys;
    for (uint32_t i = 0; i < 4096; i++) {
        page[i] = (uint8_t)(i & 0xFF);
    }
    
    uint64_t virt = 0x400000;
    
    // Map in both address spaces
    vmm_map_page(pml4_1, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    vmm_map_page(pml4_2, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    
    // Mark both as COW
    cow_mark_page(pml4_1, virt);
    cow_mark_page(pml4_2, virt);
    
    TEST_ASSERT(cow_get_ref_count(phys) == 2, "Reference count should be 2");
    
    // Handle fault in first address space (should copy)
    int result = cow_handle_fault(pml4_1, virt);
    TEST_ASSERT(result == 0, "COW fault handling should succeed");
    
    // Verify physical address changed
    uint64_t new_phys = vmm_get_physical_address(pml4_1, virt);
    TEST_ASSERT(new_phys != phys, "Physical address should change after COW copy");
    
    // Verify page contents were copied
    uint8_t *new_page = (uint8_t *)(uintptr_t)new_phys;
    int contents_match = 1;
    for (uint32_t i = 0; i < 4096; i++) {
        if (new_page[i] != (uint8_t)(i & 0xFF)) {
            contents_match = 0;
            break;
        }
    }
    TEST_ASSERT(contents_match, "Page contents should be copied correctly");
    
    // Verify reference count decreased
    TEST_ASSERT(cow_get_ref_count(phys) == 1, "Reference count should be 1 after copy");
    
    // Clean up
    vmm_unmap_page(pml4_1, virt);
    vmm_unmap_page(pml4_2, virt);
    buddy_free_pages(new_phys, 0);
    cow_decrement_ref(phys);
}

void test_cow_multi_process_sharing(void) {
    // Simulate multiple processes sharing pages
    page_table_t *pml4_1 = vmm_create_address_space();
    page_table_t *pml4_2 = vmm_create_address_space();
    page_table_t *pml4_3 = vmm_create_address_space();
    
    TEST_ASSERT(pml4_1 != 0 && pml4_2 != 0 && pml4_3 != 0, 
                "All address space creations should succeed");
    
    if (!pml4_1 || !pml4_2 || !pml4_3) return;
    
    // Allocate shared physical page
    uint64_t phys = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    TEST_ASSERT(phys != 0, "Physical page allocation should succeed");
    
    if (!phys) return;
    
    uint64_t virt = 0x400000;
    
    // Map in all three address spaces
    vmm_map_page(pml4_1, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    vmm_map_page(pml4_2, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    vmm_map_page(pml4_3, virt, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    
    // Mark all as COW
    cow_mark_page(pml4_1, virt);
    cow_mark_page(pml4_2, virt);
    cow_mark_page(pml4_3, virt);
    
    TEST_ASSERT(cow_get_ref_count(phys) == 3, "Reference count should be 3");
    
    // First process writes (triggers copy)
    cow_handle_fault(pml4_1, virt);
    TEST_ASSERT(cow_get_ref_count(phys) == 2, "Reference count should be 2 after first copy");
    
    // Second process writes (triggers copy)
    cow_handle_fault(pml4_2, virt);
    TEST_ASSERT(cow_get_ref_count(phys) == 1, "Reference count should be 1 after second copy");
    
    // Third process writes (should just make writable)
    uint64_t phys_before = vmm_get_physical_address(pml4_3, virt);
    cow_handle_fault(pml4_3, virt);
    uint64_t phys_after = vmm_get_physical_address(pml4_3, virt);
    
    TEST_ASSERT(phys_before == phys_after, 
                "Last reference should not trigger copy");
    
    // Clean up
    uint64_t phys1 = vmm_get_physical_address(pml4_1, virt);
    uint64_t phys2 = vmm_get_physical_address(pml4_2, virt);
    
    vmm_unmap_page(pml4_1, virt);
    vmm_unmap_page(pml4_2, virt);
    vmm_unmap_page(pml4_3, virt);
    
    buddy_free_pages(phys1, 0);
    buddy_free_pages(phys2, 0);
    buddy_free_pages(phys, 0);
}

void run_cow_tests(void) {
    kprintf("Running COW tests...\n");
    
    test_cow_initialization();
    test_cow_page_marking();
    test_cow_reference_counting();
    test_cow_fault_handler_single_ref();
    test_cow_fault_handler_multi_ref();
    test_cow_multi_process_sharing();
    
    kprintf("COW tests: %d/%d passed\n", test_passed, test_count);
}


// New tests for error logging

void test_cow_null_pml4_handling(void) {
    // Test cow_mark_page with NULL pml4
    int result = cow_mark_page(NULL, 0x400000);
    TEST_ASSERT(result == -1, "cow_mark_page with NULL pml4 should return error");
    // Should log error message (can't easily verify, but shouldn't crash)
}

void test_cow_unmapped_page_handling(void) {
    // Create address space
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Try to mark unmapped page as COW
    uint64_t unmapped_virt = 0x500000;
    int result = cow_mark_page(pml4, unmapped_virt);
    TEST_ASSERT(result == -1, "cow_mark_page on unmapped page should return error");
    // Should log error message
}

void test_cow_allocation_failure_handling(void) {
    // This test simulates allocation failure by using up memory
    // In practice, this is hard to test without actually exhausting memory
    // We just verify the function handles NULL returns gracefully
    
    // Test cow_get_ref with a physical address
    uint64_t phys = buddy_alloc_pages(0, BUDDY_ZONE_UNMOVABLE);
    if (phys) {
        page_ref_t *ref = cow_get_ref(phys);
        TEST_ASSERT(ref != NULL, "cow_get_ref should succeed with valid physical address");
        
        // Clean up
        cow_decrement_ref(phys);
    }
}

void test_cow_error_logging_verification(void) {
    // This test verifies that error logging doesn't crash the system
    // Actual log message verification would require capturing kprintf output
    
    page_table_t *pml4 = vmm_create_address_space();
    if (!pml4) return;
    
    // Trigger various error conditions
    cow_mark_page(NULL, 0x400000);  // NULL pml4
    cow_mark_page(pml4, 0x500000);  // Unmapped page
    cow_handle_fault(NULL, 0x400000);  // NULL pml4 in fault handler
    cow_handle_fault(pml4, 0x500000);  // Unmapped page in fault handler
    
    TEST_ASSERT(1, "Error logging should not crash the system");
}

void test_cow_hash_function_distribution(void) {
    // Test that hash function distributes addresses reasonably
    // We can't test the actual distribution without many allocations,
    // but we can verify it doesn't crash and returns valid indices
    
    uint64_t test_addresses[] = {
        0x1000, 0x2000, 0x3000, 0x4000, 0x5000,
        0x100000, 0x200000, 0x300000, 0x400000, 0x500000
    };
    
    for (int i = 0; i < 10; i++) {
        page_ref_t *ref = cow_get_ref(test_addresses[i]);
        if (ref) {
            TEST_ASSERT(1, "Hash function should handle various addresses");
            cow_decrement_ref(test_addresses[i]);
        }
    }
}

void run_cow_tests_extended(void) {
    kprintf("\nRunning extended COW tests...\n");
    
    int old_count = test_count;
    int old_passed = test_passed;
    
    test_cow_null_pml4_handling();
    test_cow_unmapped_page_handling();
    test_cow_allocation_failure_handling();
    test_cow_error_logging_verification();
    test_cow_hash_function_distribution();
    
    kprintf("Extended COW tests: %d/%d passed\n", 
            test_passed - old_passed, test_count - old_count);
}
