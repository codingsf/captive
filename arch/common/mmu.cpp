#include <mmu.h>
#include <cpu.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;

static const char *mem_access_types[] = { "read", "write", "fetch", "read-user", "write-user" };
static const char *mem_fault_types[] = { "none", "read", "write", "fetch" };

MMU::MMU(CPU& cpu) : _cpu(cpu)
{
	printf("mmu: allocating guest pdps\n");

	page_map_t *pm = (page_map_t *)Memory::phys_to_virt(Memory::read_cr3());
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)Memory::phys_to_virt((pa_t)pm->entries[0].base_address());

	for (int i = 0; i < 4; i++) {
		pdp->entries[i].base_address((uint64_t)Memory::alloc_page().pa);
		pdp->entries[i].flags(0);
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
	}

	Memory::flush_tlb();
}

MMU::~MMU()
{

}

bool MMU::clear_vma()
{
	page_map_t *pm = (page_map_t *)Memory::phys_to_virt(Memory::read_cr3());
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)Memory::phys_to_virt((pa_t)pm->entries[0].base_address());

	// Clear the present map on the 4G mapping
	for (int i = 0; i < 4; i++) {
		pdp->entries[i].present(false);
	}

	// Flush the TLB
	Memory::flush_tlb();
	return true;
}

void MMU::cpu_privilege_change(bool kernel_mode)
{
	clear_vma();
}

bool MMU::handle_fault(gva_t va, const access_info& info, resolution_fault& fault)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries((va_t)(uint64_t)va, pm, pdp, pd, pt);

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

	if (!enabled()) {
		fault = NONE;

		pt->base_address((uint64_t)gpa_to_hpa((gpa_t)va));
		pt->present(true);
		pt->writable(true);
		pt->allow_user(true);
	} else {
		gpa_t pa;
		if (!resolve_gpa(va, pa, info, fault)) {
			return false;
		}

		if (fault == NONE) {
			// Update the corresponding page table address entry and mark it as
			// present and writable.  Note, assigning the base address will mask
			// out the bottom twelve bits of the incoming address, to ensure it's
			// a page-aligned value.
			pt->base_address((uint64_t)gpa_to_hpa(pa));
			pt->present(true);
			pt->writable(info.is_write());
			pt->allow_user(info.is_user());

			//printf("mmu: %d no fault: va=%08x pa=%08x access-type=%s rw=%d\n", _cpu.get_insns_executed(), va, pa, mem_access_types[mem_access_type], rw);
		} else {
			pt->present(false);
			//printf("mmu: %08x fault: va=%08x access-type=%s fault-type=%s\n", _cpu.read_pc(), va, mem_access_types[mem_access_type], mem_fault_types[fault]);
		}
	}

	Memory::flush_page((va_t)(uint64_t)va);
	return true;
}

