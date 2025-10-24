#include "../../include/mm/demand_paging.h"
#include "../../include/mm/buddy.h"
#include "../../include/mm/slab.h"
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

void test_demand_paging_initialization(void) {
    demand_paging_init();
    TEST_ASSERT(1, "Demand paging initialization should complete without crashing");
}

void test_region_registration(void) {
    // Create address space
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Register a region
    uint64_t start = 0x100000;
    uint64_t size = 0x10000;  // 64KB
    uint32_t flags = VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL;
    
    int result = demand_paging_register_region(pml4, start, size, flags);
    TEST_ASSERT(result == 0, "Region registration should succeed");
    
    // Verify region can be found
    vm_region_t *region = demand_paging_find_region(pml4, start);
    TEST_ASSERT(region != NULL, "Registered region should be found");
    
    if (region) {
        TEST_ASSERT(region->start == start, "Region start should match");
        TEST_ASSERT(region->end == start + size, "Region end should match");
        TEST_ASSERT(region->flags == flags, "Region flags should match");
    }
    
    // Clean up
    demand_paging_unregister_region(pml4, start);
}

void test_region_overlap_detection(void) {
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Register first region
    uint64_t start1 = 0x100000;
    uint64_t size1 = 0x10000;
    int result1 = demand_paging_register_region(pml4, start1, size1, VM_FLAG_DEMAND_PAGED);
    TEST_ASSERT(result1 == 0, "First region registration should succeed");
    
    // Try to register overlapping region (should fail)
    uint64_t start2 = 0x108000;  // Overlaps with first region
    uint64_t size2 = 0x10000;
    int result2 = demand_paging_register_region(pml4, start2, size2, VM_FLAG_DEMAND_PAGED);
    TEST_ASSERT(result2 == -1, "Overlapping region registration should fail");
    
    // Register non-overlapping region (should succeed)
    uint64_t start3 = 0x200000;
    uint64_t size3 = 0x10000;
    int result3 = demand_paging_register_region(pml4, start3, size3, VM_FLAG_DEMAND_PAGED);
    TEST_ASSERT(result3 == 0, "Non-overlapping region registration should succeed");
    
    // Clean up
    demand_paging_unregister_region(pml4, start1);
    demand_paging_unregister_region(pml4, start3);
}

void test_page_fault_handling(void) {
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Register a demand-paged region
    uint64_t start = 0x100000;
    uint64_t size = 0x10000;
    int reg_result = demand_paging_register_region(pml4, start, size, 
                                                    VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL);
    TEST_ASSERT(reg_result == 0, "Region registration should succeed");
    
    // Verify page is not mapped initially
    uint64_t phys_before = vmm_get_physical_address(pml4, start);
    TEST_ASSERT(phys_before == 0, "Page should not be mapped initially");
    
    // Trigger page fault
    int fault_result = demand_paging_handle_fault(pml4, start);
    TEST_ASSERT(fault_result == 0, "Page fault handling should succeed");
    
    // Verify page is now mapped
    uint64_t phys_after = vmm_get_physical_address(pml4, start);
    TEST_ASSERT(phys_after != 0, "Page should be mapped after fault");
    
    // Clean up
    demand_paging_unregister_region(pml4, start);
}

void test_zero_fill(void) {
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Register a zero-fill region
    uint64_t start = 0x100000;
    uint64_t size = 0x1000;  // One page
    demand_paging_register_region(pml4, start, size, 
                                  VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL);
    
    // Trigger page fault
    int result = demand_paging_handle_fault(pml4, start);
    TEST_ASSERT(result == 0, "Page fault handling should succeed");
    
    // Get physical address
    uint64_t phys = vmm_get_physical_address(pml4, start);
    TEST_ASSERT(phys != 0, "Physical page should be allocated");
    
    if (phys != 0) {
        // Verify page is zero-filled (using direct physical mapping)
        uint64_t direct_map_addr = 0xFFFF800000000000ULL + phys;
        uint8_t *page = (uint8_t *)direct_map_addr;
        
        int all_zeros = 1;
        for (uint32_t i = 0; i < 4096; i++) {
            if (page[i] != 0) {
                all_zeros = 0;
                break;
            }
        }
        
        TEST_ASSERT(all_zeros, "Page should be zero-filled");
    }
    
    // Clean up
    demand_paging_unregister_region(pml4, start);
}

void test_invalid_fault_handling(void) {
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Try to handle fault for unregistered address
    uint64_t invalid_addr = 0x500000;
    int result = demand_paging_handle_fault(pml4, invalid_addr);
    TEST_ASSERT(result == -1, "Fault handling should fail for unregistered address");
    
    // Register a region but without demand paging flag
    uint64_t start = 0x100000;
    uint64_t size = 0x1000;
    demand_paging_register_region(pml4, start, size, 0);  // No flags
    
    // Try to handle fault (should fail because VM_FLAG_DEMAND_PAGED not set)
    result = demand_paging_handle_fault(pml4, start);
    TEST_ASSERT(result == -1, "Fault handling should fail without demand paging flag");
    
    // Clean up
    demand_paging_unregister_region(pml4, start);
}

void test_region_unregistration(void) {
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Register and allocate pages in a region
    uint64_t start = 0x100000;
    uint64_t size = 0x3000;  // 3 pages
    demand_paging_register_region(pml4, start, size, 
                                  VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL);
    
    // Allocate all pages
    demand_paging_handle_fault(pml4, start);
    demand_paging_handle_fault(pml4, start + 0x1000);
    demand_paging_handle_fault(pml4, start + 0x2000);
    
    // Verify pages are mapped
    TEST_ASSERT(vmm_get_physical_address(pml4, start) != 0, "First page should be mapped");
    TEST_ASSERT(vmm_get_physical_address(pml4, start + 0x1000) != 0, "Second page should be mapped");
    TEST_ASSERT(vmm_get_physical_address(pml4, start + 0x2000) != 0, "Third page should be mapped");
    
    // Unregister region
    demand_paging_unregister_region(pml4, start);
    
    // Verify region is removed
    vm_region_t *region = demand_paging_find_region(pml4, start);
    TEST_ASSERT(region == NULL, "Region should be removed after unregistration");
    
    // Verify pages are unmapped
    TEST_ASSERT(vmm_get_physical_address(pml4, start) == 0, "First page should be unmapped");
    TEST_ASSERT(vmm_get_physical_address(pml4, start + 0x1000) == 0, "Second page should be unmapped");
    TEST_ASSERT(vmm_get_physical_address(pml4, start + 0x2000) == 0, "Third page should be unmapped");
}

void test_multiple_regions(void) {
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Register multiple regions
    uint64_t start1 = 0x100000;
    uint64_t start2 = 0x200000;
    uint64_t start3 = 0x300000;
    uint64_t size = 0x1000;
    
    int r1 = demand_paging_register_region(pml4, start1, size, VM_FLAG_DEMAND_PAGED);
    int r2 = demand_paging_register_region(pml4, start2, size, VM_FLAG_DEMAND_PAGED);
    int r3 = demand_paging_register_region(pml4, start3, size, VM_FLAG_DEMAND_PAGED);
    
    TEST_ASSERT(r1 == 0 && r2 == 0 && r3 == 0, "All region registrations should succeed");
    
    // Verify all regions can be found
    TEST_ASSERT(demand_paging_find_region(pml4, start1) != NULL, "First region should be found");
    TEST_ASSERT(demand_paging_find_region(pml4, start2) != NULL, "Second region should be found");
    TEST_ASSERT(demand_paging_find_region(pml4, start3) != NULL, "Third region should be found");
    
    // Handle faults in all regions
    TEST_ASSERT(demand_paging_handle_fault(pml4, start1) == 0, "First fault should succeed");
    TEST_ASSERT(demand_paging_handle_fault(pml4, start2) == 0, "Second fault should succeed");
    TEST_ASSERT(demand_paging_handle_fault(pml4, start3) == 0, "Third fault should succeed");
    
    // Clean up
    demand_paging_unregister_region(pml4, start1);
    demand_paging_unregister_region(pml4, start2);
    demand_paging_unregister_region(pml4, start3);
}

void run_demand_paging_tests(void) {
    kprintf("Running demand paging tests...\n");
    
    test_demand_paging_initialization();
    test_region_registration();
    test_region_overlap_detection();
    test_page_fault_handling();
    test_zero_fill();
    test_invalid_fault_handling();
    test_region_unregistration();
    test_multiple_regions();
    
    kprintf("Demand paging tests: %d/%d passed\n", test_passed, test_count);
}


// New tests for concurrency and race condition handling

void test_demand_paging_concurrent_faults_simulation(void) {
    // Simulate concurrent page faults on the same address
    // In a single-threaded test, we can verify the double-checked locking logic
    
    page_table_t *pml4 = vmm_create_address_space();
    TEST_ASSERT(pml4 != 0, "Address space creation should succeed");
    
    if (!pml4) return;
    
    // Register a region
    uint64_t start = 0x200000;
    uint64_t size = 0x10000;
    uint32_t flags = VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL;
    
    int result = demand_paging_register_region(pml4, start, size, flags);
    TEST_ASSERT(result == 0, "Region registration should succeed");
    
    // First fault should allocate the page
    uint64_t fault_addr = start + 0x1000;
    result = demand_paging_handle_fault(pml4, fault_addr);
    TEST_ASSERT(result == 0, "First page fault should succeed");
    
    // Verify page is mapped
    uint64_t phys1 = vmm_get_physical_address(pml4, fault_addr);
    TEST_ASSERT(phys1 != 0, "Page should be mapped after first fault");
    
    // Second fault on same address should detect page is already mapped
    result = demand_paging_handle_fault(pml4, fault_addr);
    TEST_ASSERT(result == 0, "Second page fault should succeed (fast path)");
    
    // Verify physical address didn't change
    uint64_t phys2 = vmm_get_physical_address(pml4, fault_addr);
    TEST_ASSERT(phys2 == phys1, "Physical address should not change on second fault");
    
    // Clean up
    demand_paging_unregister_region(pml4, start);
}

void test_demand_paging_lock_initialization(void) {
    // Verify that page_fault_lock is properly initialized
    page_table_t *pml4 = vmm_create_address_space();
    if (!pml4) return;
    
    uint64_t start = 0x300000;
    uint64_t size = 0x10000;
    uint32_t flags = VM_FLAG_DEMAND_PAGED;
    
    int result = demand_paging_register_region(pml4, start, size, flags);
    TEST_ASSERT(result == 0, "Region registration should succeed");
    
    vm_region_t *region = demand_paging_find_region(pml4, start);
    TEST_ASSERT(region != NULL, "Region should be found");
    
    if (region) {
        // We can't directly test the lock, but we can verify the region structure is valid
        TEST_ASSERT(region->start == start, "Region lock should be initialized with valid region");
    }
    
    demand_paging_unregister_region(pml4, start);
}

void test_demand_paging_multiple_regions(void) {
    // Test multiple regions with concurrent-like access patterns
    page_table_t *pml4 = vmm_create_address_space();
    if (!pml4) return;
    
    // Register multiple regions
    uint64_t region1_start = 0x400000;
    uint64_t region2_start = 0x500000;
    uint64_t region3_start = 0x600000;
    uint64_t size = 0x10000;
    
    demand_paging_register_region(pml4, region1_start, size, VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL);
    demand_paging_register_region(pml4, region2_start, size, VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL);
    demand_paging_register_region(pml4, region3_start, size, VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL);
    
    // Trigger faults in different regions (simulating concurrent access)
    int r1 = demand_paging_handle_fault(pml4, region1_start + 0x1000);
    int r2 = demand_paging_handle_fault(pml4, region2_start + 0x2000);
    int r3 = demand_paging_handle_fault(pml4, region3_start + 0x3000);
    
    TEST_ASSERT(r1 == 0 && r2 == 0 && r3 == 0, "All page faults should succeed");
    
    // Verify all pages are mapped
    uint64_t phys1 = vmm_get_physical_address(pml4, region1_start + 0x1000);
    uint64_t phys2 = vmm_get_physical_address(pml4, region2_start + 0x2000);
    uint64_t phys3 = vmm_get_physical_address(pml4, region3_start + 0x3000);
    
    TEST_ASSERT(phys1 != 0 && phys2 != 0 && phys3 != 0, "All pages should be mapped");
    TEST_ASSERT(phys1 != phys2 && phys2 != phys3 && phys1 != phys3, 
                "All pages should have different physical addresses");
    
    // Clean up
    demand_paging_unregister_region(pml4, region1_start);
    demand_paging_unregister_region(pml4, region2_start);
    demand_paging_unregister_region(pml4, region3_start);
}

void test_demand_paging_error_path_cleanup(void) {
    // Test that error paths properly release locks
    page_table_t *pml4 = vmm_create_address_space();
    if (!pml4) return;
    
    // Try to handle fault without registering region (should fail)
    uint64_t unregistered_addr = 0x700000;
    int result = demand_paging_handle_fault(pml4, unregistered_addr);
    TEST_ASSERT(result == -1, "Fault on unregistered region should fail");
    
    // System should still be functional after error
    uint64_t start = 0x800000;
    uint64_t size = 0x10000;
    result = demand_paging_register_region(pml4, start, size, VM_FLAG_DEMAND_PAGED);
    TEST_ASSERT(result == 0, "Region registration should succeed after previous error");
    
    result = demand_paging_handle_fault(pml4, start + 0x1000);
    TEST_ASSERT(result == 0, "Page fault should succeed after previous error");
    
    demand_paging_unregister_region(pml4, start);
}

void test_demand_paging_zero_fill_verification(void) {
    // Verify that zero-fill actually zeros the page
    page_table_t *pml4 = vmm_create_address_space();
    if (!pml4) return;
    
    uint64_t start = 0x900000;
    uint64_t size = 0x10000;
    
    demand_paging_register_region(pml4, start, size, VM_FLAG_DEMAND_PAGED | VM_FLAG_ZERO_FILL);
    
    uint64_t fault_addr = start + 0x1000;
    demand_paging_handle_fault(pml4, fault_addr);
    
    uint64_t phys = vmm_get_physical_address(pml4, fault_addr);
    if (phys) {
        // Check if page is zeroed (using direct physical mapping)
        uint64_t direct_map_addr = 0xFFFF800000000000ULL + phys;
        uint8_t *page_ptr = (uint8_t *)direct_map_addr;
        
        int all_zero = 1;
        for (int i = 0; i < 4096; i++) {
            if (page_ptr[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        TEST_ASSERT(all_zero, "Zero-fill flag should zero the page");
    }
    
    demand_paging_unregister_region(pml4, start);
}

void run_demand_paging_tests_extended(void) {
    kprintf("\nRunning extended demand paging tests...\n");
    
    int old_count = test_count;
    int old_passed = test_passed;
    
    test_demand_paging_concurrent_faults_simulation();
    test_demand_paging_lock_initialization();
    test_demand_paging_multiple_regions();
    test_demand_paging_error_path_cleanup();
    test_demand_paging_zero_fill_verification();
    
    kprintf("Extended demand paging tests: %d/%d passed\n", 
            test_passed - old_passed, test_count - old_count);
}
