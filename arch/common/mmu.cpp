#include <mmu.h>
#include <cpu.h>
#include <mm.h>
#include <printf.h>
#include <profile/image.h>

using namespace captive::arch;

static const char *mem_access_types[] = { "read", "write", "fetch" };
static const char *mem_access_modes[] = { "user", "kernel" };
static const char *mem_fault_types[] = { "none", "read", "write", "fetch" };

MMU::MMU(CPU& cpu) : _cpu(cpu)
{
	//printf("mmu: allocating guest pdps\n");

	page_map_t *pm = (page_map_t *)Memory::phys_to_virt(Memory::read_cr3());
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)Memory::phys_to_virt((pa_t)pm->entries[0].base_address());

	for (int i = 0; i < 4; i++) {
		pdp->entries[i].base_address((uint64_t)Memory::alloc_page().pa);
		pdp->entries[i].flags(0);
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
		pdp->entries[i].allow_user(true);
	}

	Memory::flush_tlb();
}

MMU::~MMU()
{

}

void MMU::set_page_executed(gva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries((va_t)(uint64_t)va, pm, pdp, pd, pt);

	// Avoid a page flush for pages already marked as executed.
	if (!pt->executed()) {
		//printf("mmu: setting page %08x as executed\n", va);
		pt->executed(true);
		pt->writable(false);

		Memory::flush_page((va_t)(uint64_t)va);
	}
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

	//printf("mmu: vma cleared\n");
	return true;
}

void MMU::cpu_privilege_change(bool kernel_mode)
{
	//clear_vma();
}

bool MMU::handle_fault(gva_t va, gpa_t& out_pa, const access_info& info, resolution_fault& fault)
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
			base->entries[i].device(false);
			base->entries[i].allow_user(false);
		}

		// Set the PRESENT flag for the page table.
		pd->present(true);
	}

	// If the fault happened because of a device access, catch this early
	// before we go over the lookup rigmaroll.
	if (pt->device()) {
		out_pa = (gpa_t)((pt->base_address() & 0xffffffff) | (va & 0xfff));
		goto handle_device;
	}

	if (pt->present() && info.is_write() && pt->executed()) {
		//printf("mmu: execution fault phys=%08x, virt=%08x jit=%d\n", pt->base_address(), va, _cpu.executing_translation());

		// Clear the EXECUTED flag and invalidate the page, so that translations
		// can be discarded.
		pt->executed(false);
		_cpu.invalidate_executed_page((pa_t)(pt->base_address()), (va_t)((uint64_t)va & ~0xfffULL));

		// Now, if we are executing in the JIT, and we are executing code in the page
		// which was invalidated - i.e. self-modifying code in the same page, then we
		// need to do something special.  ASSERT!
		if (_cpu.executing_translation() && ((uint64_t)_cpu.read_pc() & ~0xfffULL) == ((uint64_t)va & ~0xfffULL)) {
			assert(false && "write to same executed page whilst in jitted code");
		}
	}

	if (!enabled()) {
		fault = NONE;

		pt->base_address((uint64_t)gpa_to_hpa((gpa_t)va));
		pt->present(true);
		pt->allow_user(true);
		pt->writable(true);

		if (is_device((gpa_t)va)) {
			pt->device(true);
			pt->present(false);
		}

		if (pt->device()) {
			out_pa = (gpa_t)va;
			goto handle_device;
		}
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
			pt->allow_user(info.is_user());
			pt->writable(info.is_write());

			if (is_device(pa)) {
				pt->device(true);
				pt->present(false);

				out_pa = pa;
				goto handle_device;
			}

			//printf("mmu: %08x no fault: va=%08x pa=%08x type=%s mode=%s\n", _cpu.read_pc(), va, pa, mem_access_types[info.type], mem_access_modes[info.mode]);
		} else {
			pt->present(false);
			pt->device(false);
			//printf("mmu: %08x fault: va=%08x type=%s mode=%s fault-type=%s\n", _cpu.read_pc(), va, mem_access_types[info.type], mem_access_modes[info.mode], mem_fault_types[fault]);
		}
	}

	Memory::flush_page((va_t)(uint64_t)va);
	return true;

handle_device:
	Memory::flush_page((va_t)(uint64_t)va);
	fault = DEVICE_FAULT;

	return true;
}

bool MMU::is_device(gpa_t gpa)
{
	/*if (gpa >= 0x101e2000 && gpa < 0x101e3000) return true;
	if (gpa >= 0x101e3000 && gpa < 0x101e4000) return true;
	if (gpa >= 0x10140000 && gpa < 0x10141000) return true;
	if (gpa >= 0x10000000 && gpa < 0x10001000) return true;
	if (gpa >= 0x101f1000 && gpa < 0x101f2000) return true;*/
	//if (gpa >= 0x11001000 && gpa < 0x11002000) return true;

	return false;
}

bool MMU::virt_to_phys(gva_t va, gpa_t& pa)
{
	access_info info;
	resolution_fault fault;

	info.type = ACCESS_FETCH;
	info.mode = _cpu.kernel_mode() ? ACCESS_KERNEL : ACCESS_USER;

	return resolve_gpa(va, pa, info, fault, false);
}