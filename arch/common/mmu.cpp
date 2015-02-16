#include <mmu.h>
#include <printf.h>

#include "include/mmu.h"

using namespace captive::arch;

unsigned long MMU::__force_order;

MMU::MMU(Environment& env) : _env(env), pml4_phys(0x1000), pml4(NULL)
{
	pml4 = (pm_t)phys_to_virt(pml4_phys);
}

MMU::~MMU()
{

}

bool MMU::clear_vma()
{
	pm_t pm;
	pdp_t pdp;
	pd_t pd;
	pt_t pt;

	va_entries(0, pm, pdp, pd, pt);
	printf("mmu: clear vma: %p (%x) %p (%x) %p (%x) %p (%x)\n", pm, *pm, pdp, *pdp, pd, *pd, pt, *pt);

	// Clear flags on the 4G mapping.
	for (int pdp_idx = 0; pdp_idx < 4; pdp_idx++) {
		pdp[pdp_idx] &= ~0xffdULL; // Clear all flags, except the RW bit
	}

	// Flush the TLB
	flush_tlb();

	return true;
}

bool MMU::install_phys_vma()
{
	pm_t pm;
	pdp_t pdp;
	pd_t pd;
	pt_t pt;

	va_entries(0, pm, pdp, pd, pt);
	printf("mmu: install phys mem: %p %p %p %p\n", pm, pdp, pd, pt);

	// Re-assert 1-1 mapping for physical memory VMAs
	for (int pdp_idx = 0; pdp_idx < 4; pdp_idx++) {
		printf("mmu: pdp: %x\n", pdp[pdp_idx]);
		pdp[pdp_idx] |= 1;				// Reassociate PRESENT bit
		printf("mmu: pdp: %x\n", pdp[pdp_idx]);
	}

	// Flush TLB
	flush_tlb();

	return true;
}

void MMU::map_page(page *pml4, uint64_t va, uint64_t pa)
{
	uint16_t pm, pdp, pd, pt;
	va_idx(va, pm, pdp, pd, pt);
}

void MMU::unmap_page(page *pml4, uint64_t va)
{
	uint16_t pm, pdp, pd, pt;
	va_idx(va, pm, pdp, pd, pt);
}

bool MMU::handle_fault(uint64_t va)
{
	pm_t pm;
	pdp_t pdp;
	pd_t pd;
	pt_t pt;

	va_entries(va, pm, pdp, pd, pt);

	if (!(*pm & 1)) {
		// The associated Page Directory Pointer Table is not marked as
		// present, so invalidate the page directory pointer table, and
		// mark it as present.

		// Determine the base address of the page directory pointer table.
		pdp_t base = (pdp_t)((uint64_t)pdp & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base[i] &= ~0xffdULL;
		}

		// Set the PRESENT flag for the page directory pointer table.
		*pm |= 1;
	}

	if (!(*pdp & 1)) {
		// The associated Page Directory Table is not marked as present,
		// so invalidate the page directory table and mark it as
		// present.

		// Determine the base address of the page directory table.
		pd_t base = (pd_t)((uint64_t)pd & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base[i] &= ~0xffdULL;
		}

		// Set the PRESENT flag for the page directory table.
		*pdp |= 1;
	}

	if (!(*pd & 1)) {
		// The associated Page Table is not marked as present, so
		// invalidate the page table and mark it as present.

		// Determine the base address of the page table.
		pt_t base = (pt_t)((uint64_t)pt & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base[i] &= ~0xffdULL;
		}

		// Set the PRESENT flag for the page table.
		*pd |= 1;
	}

	gpa_t pa;
	if (!resolve_gpa((gva_t)va, pa)) {
		return false;
	}

	// Update the corresponding page table address entry and mark it as
	// present and writable.
	*pt = 0x100000000 | pa | 3;

	return true;
}
