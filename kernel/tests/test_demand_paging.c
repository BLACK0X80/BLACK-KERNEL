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

