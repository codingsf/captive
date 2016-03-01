#include <mmu.h>
#include <cpu.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;

#define ITLB_SIZE	8192

static struct {
	gva_t tag;
	gpa_t value;
} itlb[ITLB_SIZE];

MMU::MMU(CPU& cpu) : _cpu(cpu), _context_id(0)
{
	//printf("mmu: allocating guest pdps\n");

	page_map_t *pm = (page_map_t *)HPA_TO_HVA(CR3);
	page_dir_ptr_t *pdp0 = (page_dir_ptr_t *)HPA_TO_HVA(pm->entries[0].base_address());
	page_dir_ptr_t *pdp1 = (page_dir_ptr_t *)HPA_TO_HVA(pm->entries[1].base_address());

	// Lower 4G and emulated 4G
	for (int i = 0; i < 4; i++) {
		pdp0->entries[i].base_address(Memory::alloc_pgt());
		pdp0->entries[i].flags(0);
		pdp0->entries[i].present(false);
		pdp0->entries[i].writable(true);
		pdp0->entries[i].allow_user(true);
		
		pdp1->entries[i].base_address(Memory::alloc_pgt());
		pdp1->entries[i].flags(0);
		pdp1->entries[i].present(false);
		pdp1->entries[i].writable(true);
		pdp1->entries[i].allow_user(true);
	}

	Memory::flush_tlb();
	
	for (int i = 0; i < ITLB_SIZE; i++) {
		itlb[i].tag = 0;
	}
}

MMU::~MMU()
{

}

void MMU::context_id(uint32_t ctxid)
{
	_context_id = ctxid;
	//printf("mmu: change context id: %x\n", _context_id);
}


void MMU::set_page_device(hva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->device(true);
}

bool MMU::is_page_device(hva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	return pt->device();
}

void MMU::set_page_executed(hva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->executed(true);
}

void MMU::clear_page_executed(hva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->executed(false);
}

bool MMU::is_page_executed(hva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	return pt->executed();
}

bool MMU::clear_if_page_executed(hva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);

	if (!pt->executed()) return false;
	
	pt->executed(false);
	return true;
}

bool MMU::is_page_dirty(hva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);

	return pt->dirty();
}

void MMU::set_page_dirty(hva_t va, bool dirty)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->dirty(dirty);
}

uint32_t MMU::page_checksum(hva_t va)
{
	uint32_t checksum = 0;
	for (int i = 0; i < 0x400; i++) {
		checksum ^= ((uint32_t *)va)[i];
	}

	return checksum;
}

void MMU::invalidate_virtual_mappings()
{
	page_map_t *pm = (page_map_t *)HPA_TO_HVA(CR3);
	page_dir_ptr_t *pdp0 = (page_dir_ptr_t *)HPA_TO_HVA(pm->entries[0].base_address());
	page_dir_ptr_t *pdp1 = (page_dir_ptr_t *)HPA_TO_HVA(pm->entries[1].base_address());

	// Clear the present map and re-enable writing on the 4G + E4G mapping.
	for (int i = 0; i < 4; i++) {
		pdp0->entries[i].present(false);
		pdp0->entries[i].writable(true);
		pdp1->entries[i].present(false);
		pdp1->entries[i].writable(true);
	}

	// Flush the TLB
	Memory::flush_tlb();

	// Notify the CPU to invalidate virtual mappings
	_cpu.invalidate_virtual_mappings();
	
	for (int i = 0; i < ITLB_SIZE; i++) {
		itlb[i].tag = 0;
	}
}

void MMU::invalidate_virtual_mapping(gva_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;
	
	Memory::get_va_table_entries((hva_t)va, pm, pdp, pd, pt);
	pt->present(false);

	Memory::flush_page((hva_t)va);
	
	Memory::get_va_table_entries(GVA_TO_EMULATED_HVA(va), pm, pdp, pd, pt);
	pt->present(false);

	Memory::flush_page(GVA_TO_EMULATED_HVA(va));
	
	// Notify the CPU to invalidate this virtual mapping
	_cpu.invalidate_virtual_mapping(va);
	
	itlb[(va >> 12) % ITLB_SIZE].tag = 0;
}

void MMU::invalidate_virtual_mapping_by_context_id(uint32_t context_id)
{
	invalidate_virtual_mappings();
	//printf("mmu: invalidate by context id: %x\n", context_id);
}

void MMU::disable_writes()
{
	page_map_t *pm = (page_map_t *)HPA_TO_HVA(CR3);
	page_dir_ptr_t *pdp0 = (page_dir_ptr_t *)HPA_TO_HVA(pm->entries[0].base_address());
	page_dir_ptr_t *pdp1 = (page_dir_ptr_t *)HPA_TO_HVA(pm->entries[1].base_address());

	// Clear the writable flag on the 4G + E4G mapping
	for (int i = 0; i < 4; i++) {
		pdp0->entries[i].writable(false);
		pdp1->entries[i].writable(false);
	}
	
	// Flush the TLB
	if (in_kernel_mode()) {
		Memory::flush_tlb();
	} else {
		asm volatile("int $0x83\n" ::: "rax");
	}
}

bool MMU::handle_fault(struct resolution_context& rc)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	hva_t host_va;
	if (rc.emulate_user) {
		host_va = GVA_TO_EMULATED_HVA(rc.va);
	} else {
		host_va = (hva_t)(rc.va);
	}
	
	Memory::get_va_table_entries(host_va, pm, pdp, pd, pt);

	/*printf("mmu: handle fault: va=%x hva=%lx ctxid=%x pme(%p)=(%lx) pdpe(%p)=(%lx) pde(%p)=(%lx) pte(%p)=(%lx)\n",
			rc.va,
			host_va,
			*cpu().tagged_registers().CTXID,
			pm, pm->data,
			pdp, pdp->data,
			pd, pd->data,
			pt, pt->data);*/

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

	if (!pdp->present() || !pdp->writable()) {
		// The associated Page Directory Table is not marked as present,
		// so invalidate the page directory table and mark it as
		// present.

		// Determine the base address of the page directory table.
		page_dir_t *base = (page_dir_t *)((uint64_t)pd & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			if (!pdp->present()) base->entries[i].present(false);
			if (!pdp->writable()) base->entries[i].writable(false);
		}

		// Set the PRESENT flag for the page directory table.
		pdp->present(true);
		pdp->writable(true);
	}

	if (!pd->present() || !pd->writable()) {
		// The associated Page Table is not marked as present, so
		// invalidate the page table and mark it as present.

		// Determine the base address of the page table.
		page_table_t *base = (page_table_t *)((uint64_t)pt & ~0xfffULL);

		// Loop over each entry and clear the flags.
		if (!pd->present()) {
			for (int i = 0; i < 0x200; i++) {
				base->entries[i].flags(0);
				
				/*present(false);
				base->entries[i].device(false);
				base->entries[i].allow_user(false);
				base->entries[i].executable(false);*/
			}
		} else if (!pd->writable()) {
			for (int i = 0; i < 0x200; i++) {
				base->entries[i].writable(false);
			}
		}

		// Set the PRESENT flag for the page table.
		pd->present(true);
		pd->writable(true);
	}

	// If the fault happened because of a device access, catch this early
	// before we go over the lookup rigmaroll.
	if (pt->device()) {
		rc.pa = (gpa_t)((pt->base_address() & 0xffffffff) | (rc.va & 0xfff));
		goto handle_device;
	}

	if (!enabled()) {
		rc.fault = NONE;
		rc.pa = (gpa_t)rc.va;
		rc.allowed_permissions = ALL_READ_WRITE_FETCH;

		pt->base_address(GPA_TO_HPA(rc.pa));
		pt->present(true);
		pt->allow_user(true);
		pt->writable(true);
		
		//printf("mmu: %08x disabled: va=%08x pa=%08x hpa=%lx, hva=%lx\n", _cpu.read_pc(), rc.va, rc.pa, pt->base_address(), GPA_TO_HVA(pt->base_address()));

		if (is_page_device(GPA_TO_HVA(pt->base_address()))) {
			pt->device(true);
			pt->present(false);

			goto handle_device;
		}
	} else {
		if (!resolve_gpa(rc, true)) {
			return false;
		}
		
		if (rc.fault == NONE) {
			// Update the corresponding page table address entry and mark it as
			// present and writable.  Note, assigning the base address will mask
			// out the bottom twelve bits of the incoming address, to ensure it's
			// a page-aligned value.
			pt->base_address((uint64_t)GPA_TO_HPA(rc.pa));
			pt->present(true);
			pt->allow_user(!!(rc.allowed_permissions & (USER_READ | USER_WRITE | USER_FETCH)));
			pt->writable(!!(rc.allowed_permissions & (USER_WRITE | KERNEL_WRITE)));
			pt->executable(!!(rc.allowed_permissions & (USER_FETCH | KERNEL_FETCH)));

			if (is_page_device(GPA_TO_HVA(rc.pa))) {
				pt->device(true);
				pt->present(false);

				Memory::flush_page(host_va);
				goto handle_device;
			}

			/*printf("mmu: [%08x] no fault: va=%08x, pa=%08x, rperms=%x, aperms=%x, ctxid=%x\n",
					_cpu.read_pc(),
					rc.va,
					rc.pa,
					rc.requested_permissions,
					rc.allowed_permissions,
					*cpu().tagged_registers().CTXID);*/
		} else {
			pt->present(false);
			pt->device(false);
			//pt->dirty(true);
			//printf("mmu: %08x fault: va=%08x type=%s mode=%s fault-type=%s\n", _cpu.read_pc(), va, mem_access_types[info.type], mem_access_modes[info.mode], mem_fault_types[fault]);
		}

		//printf("mmu: %08x enabled: va=%08x pa=%08x hpa=%lx, hva=%lx, permissions=%02x fault=%d\n", _cpu.read_pc(), rc.va, rc.pa, pt->base_address(), GPA_TO_HVA(pt->base_address()), rc.allowed_permissions, rc.fault);
	}

	if (pt->present() && rc.is_write() && rc.fault == NONE) {
		if (clear_if_page_executed(GPA_TO_HVA(pt->base_address()))) {
			cpu().invalidate_translation(pt->base_address(), (hva_t)rc.va);

			//printf("PC: %08x, VA: %08x\n", _cpu.read_pc(), (uint32_t)va);
			if ((_cpu.read_pc() & ~0xfff) == (uint32_t)(rc.va & ~0xfff)) {
				rc.fault = SMC_FAULT;
			}
		}
	}

	Memory::flush_page(host_va);
	return true;

handle_device:
	rc.fault = DEVICE_FAULT;
	return true;
}

bool MMU::translate_fetch(struct resolution_context& rc)
{
	uint32_t va_page = rc.va >> 12;
	uint32_t cache_idx = va_page % ITLB_SIZE;
	
	if (va_page != 0 && itlb[cache_idx].tag == va_page) {
		rc.fault = NONE;
		rc.pa = itlb[cache_idx].value | (rc.va & 0xfff);
		return true;
	} else {
		rc.requested_permissions = _cpu.kernel_mode() ? KERNEL_FETCH : USER_FETCH;

		if (!resolve_gpa(rc, true))
			return false;
		
		if (rc.fault == NONE) {
			itlb[cache_idx].tag = va_page;
			itlb[cache_idx].value = rc.pa & ~0xfff;
		}
		
		return true;
	}
}
