#include <mmu.h>
#include <mm.h>
#include <printf.h>

extern volatile uint32_t mem_access_type;

using namespace captive::arch;

MMU::MMU(Environment& env) : _env(env)
{
}

MMU::~MMU()
{

}

bool MMU::clear_vma()
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(0, pm, pdp, pd, pt);

	page_dir_ptr_t *pdp_base = (page_dir_ptr_t *)pdp;

	// Clear PRESENT flag on the 4G mapping.
	for (int pdp_idx = 0; pdp_idx < 4; pdp_idx++) {
		pdp_base->entries[pdp_idx].present(false);
	}

	// Flush the TLB
	Memory::flush_tlb();

	return true;
}

void* MMU::map_guest_phys_page(gpa_t pa)
{
	return NULL;
}

void* MMU::map_guest_phys_pages(gpa_t pa, int nr)
{
	return NULL;
}

void MMU::unmap_phys_page(void* p)
{
	//
}

extern bool trace;

bool MMU::handle_fault(va_t va, resolution_fault& fault)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries((va_t)va, pm, pdp, pd, pt);

	//printf("mmu: handle fault: %x %p (%x) %p (%x) %p (%x) %p (%x)\n", va, pm, pm->data, pdp, pdp->data, pd, pd->data, pt, pt->data);

	if (!pm->present()) {
		// The associated Page Directory Pointer Table is not marked as
		// present, so invalidate the page directory pointer table, and
		// mark it as present.

		// Determine the base address of the page directory pointer table.
		page_dir_ptr_t *base = (page_dir_ptr_t *)((uint64_t)pdp & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base->entries[i].present(false);
		}

		// Set the PRESENT flag for the page directory pointer table.
		pm->present(true);
	}

	if (!pdp->present()) {
		// The associated Page Directory Table is not marked as present,
		// so invalidate the page directory table and mark it as
		// present.

		// Determine the base address of the page directory table.
		page_dir_t *base = (page_dir_t *)((uint64_t)pd & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base->entries[i].present(false);
		}

		// Set the PRESENT flag for the page directory table.
		pdp->present(true);
	}

	if (!pd->present()) {
		// The associated Page Table is not marked as present, so
		// invalidate the page table and mark it as present.

		// Determine the base address of the page table.
		page_table_t *base = (page_table_t *)((uint64_t)pt & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base->entries[i].present(false);
		}

		// Set the PRESENT flag for the page table.
		pd->present(true);
	}

	access_type type = (access_type)mem_access_type;
	gpa_t pa;

	if (!resolve_gpa((gva_t)(uint64_t)va, pa, type, fault)) {
		return false;
	}

	if (fault == NONE) {
		// Update the corresponding page table address entry and mark it as
		// present and writable.  Note, assigning the base address will mask
		// out the bottom twelve bits of the incoming address, to ensure it's
		// a page-aligned value.
		pt->base_address(0x100000000 | (uint64_t)pa);
		pt->present(true);
		pt->writable(true);
	} else {
		printf("fault %d %d %x\n", type, fault, va);
	}

	Memory::flush_page(va);
	return true;
}
