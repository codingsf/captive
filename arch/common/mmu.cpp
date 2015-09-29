#include <mmu.h>
#include <cpu.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;

static const char *mem_access_types[] = { "read", "write", "fetch" };
static const char *mem_access_modes[] = { "user", "kernel" };
static const char *mem_fault_types[] = { "none", "read", "write", "fetch" };

#define ITLB_SIZE	8192

static struct {
	gva_t tag;
	gpa_t value;
} itlb[ITLB_SIZE];

MMU::MMU(CPU& cpu) : _cpu(cpu)
{
	//printf("mmu: allocating guest pdps\n");

	page_map_t *pm = (page_map_t *)PHYS_TO_VIRT(CR3);
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)PHYS_TO_VIRT((pa_t)pm->entries[0].base_address());

	for (int i = 0; i < 4; i++) {
		pdp->entries[i].base_address((uint64_t)Memory::alloc_page().pa);
		pdp->entries[i].flags(0);
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
		pdp->entries[i].allow_user(true);
	}
	
	for (int i = 12; i < 16; i++) {
		pdp->entries[i].base_address((uint64_t)Memory::alloc_page().pa);
		pdp->entries[i].flags(0);
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
		pdp->entries[i].allow_user(true);
	}

	Memory::flush_tlb();
	
	for (int i = 0; i < ITLB_SIZE; i++) {
		itlb[i].tag = 0;
	}
}

MMU::~MMU()
{

}

void MMU::set_page_device(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->device(true);
}

bool MMU::is_page_device(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	return pt->device();
}

void MMU::set_page_executed(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->executed(true);
}

void MMU::clear_page_executed(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->executed(false);
}

bool MMU::is_page_executed(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	return pt->executed();
}

bool MMU::clear_if_page_executed(va_t va)
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

bool MMU::is_page_dirty(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);

	return pt->dirty();
}

void MMU::set_page_dirty(va_t va, bool dirty)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->dirty(dirty);
}

uint32_t MMU::page_checksum(va_t va)
{
	uint32_t checksum = 0;
	for (int i = 0; i < 0x400; i++) {
		checksum ^= ((uint32_t *)va)[i];
	}

	return checksum;
}

void MMU::invalidate_virtual_mappings()
{
	page_map_t *pm = (page_map_t *)PHYS_TO_VIRT(CR3);
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)PHYS_TO_VIRT((pa_t)pm->entries[0].base_address());

	// Clear the present map on the 4G mapping, and re-enable writing.
	for (int i = 0; i < 4; i++) {
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
	}
	
	// Clear the present map on the emulated 4G mapping, and re-enable writing.
	for (int i = 16; i < 20; i++) {
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
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
	
	Memory::get_va_table_entries((va_t)(uint64_t)va, pm, pdp, pd, pt);
	pt->present(false);

	Memory::flush_page((va_t)(uint64_t)va);
	
	Memory::get_va_table_entries((va_t)(uint64_t)(0x400000000ULL | va), pm, pdp, pd, pt);
	pt->present(false);

	Memory::flush_page((va_t)(uint64_t)(0x400000000ULL | va));
	
	// Notify the CPU to invalidate this virtual mapping
	_cpu.invalidate_virtual_mapping(va);
	
	itlb[(va >> 12) % ITLB_SIZE].tag = 0;
}

void MMU::disable_writes()
{
	page_map_t *pm = (page_map_t *)PHYS_TO_VIRT(CR3);
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)PHYS_TO_VIRT((pa_t)pm->entries[0].base_address());

	// Clear the writable flag on the 4G mapping
	for (int i = 0; i < 4; i++) {
		pdp->entries[i].writable(false);
	}
	
	// Clear the writable flag on the emulated 4G mapping
	for (int i = 16; i < 20; i++) {
		pdp->entries[i].writable(false);
	}

	// Flush the TLB
	if (in_kernel_mode()) {
		Memory::flush_tlb();
	} else {
		asm volatile("int $0x83\n" ::: "rax");
	}
}

bool MMU::handle_fault(gva_t va, gpa_t& out_pa, const access_info& info, resolution_fault& fault, bool emulate_user)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	va_t host_va;
	if (emulate_user) {
		host_va = (va_t)(0x400000000ULL | va);
	} else {
		host_va = (va_t)(va);
	}
	
	Memory::get_va_table_entries(host_va, pm, pdp, pd, pt);

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

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			if (!pd->present()) {
				base->entries[i].present(false);
				base->entries[i].device(false);
				base->entries[i].allow_user(false);
			}
			
			if (!pd->writable()) base->entries[i].writable(false);
		}

		// Set the PRESENT flag for the page table.
		pd->present(true);
		pd->writable(true);
	}

	// If the fault happened because of a device access, catch this early
	// before we go over the lookup rigmaroll.
	if (pt->device()) {
		out_pa = (gpa_t)((pt->base_address() & 0xffffffff) | (va & 0xfff));
		goto handle_device;
	}

	if (!enabled()) {
		fault = NONE;

		pt->base_address((uint64_t)gpa_to_hpa((gpa_t)va));
		pt->present(true);
		pt->allow_user(true);
		pt->writable(info.is_write());

		if (is_page_device(((va_t)(0x100000000ULL | pt->base_address())))) {
			pt->device(true);
			pt->present(false);

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

			if (is_page_device(((va_t)(0x100000000ULL | pa)))) {
				pt->device(true);
				pt->present(false);

				out_pa = pa;
				
				Memory::flush_page(host_va);
				goto handle_device;
			}

			//printf("mmu: %08x no fault: va=%08x pa=%08x type=%s mode=%s\n", _cpu.read_pc(), va, pa, mem_access_types[info.type], mem_access_modes[info.mode]);
		} else {
			pt->present(false);
			pt->device(false);
			//pt->dirty(true);
			//printf("mmu: %08x fault: va=%08x type=%s mode=%s fault-type=%s\n", _cpu.read_pc(), va, mem_access_types[info.type], mem_access_modes[info.mode], mem_fault_types[fault]);
		}
	}

	if (pt->present() && info.is_write() && fault == NONE) {
		if (clear_if_page_executed(((va_t)(0x100000000ULL | (pt->base_address() & 0xffffffff))))) {
			cpu().invalidate_translation((pa_t)pt->base_address(), (va_t)(uint64_t)va);

			//printf("PC: %08x, VA: %08x\n", _cpu.read_pc(), (uint32_t)va);
			if ((_cpu.read_pc() & ~0xfff) == (uint32_t)(va & ~0xfff)) {
				fault = SMC_FAULT;
			}
		}
	}

	Memory::flush_page(host_va);
	return true;

handle_device:
	fault = DEVICE_FAULT;
	return true;
}

bool MMU::virt_to_phys(gva_t va, gpa_t& pa, resolution_fault& fault)
{
	uint32_t va_page = va >> 12;
	uint32_t cache_idx = va_page % ITLB_SIZE;
	
	if (va_page != 0 && itlb[cache_idx].tag == va_page) {
		pa = itlb[cache_idx].value | (va & 0xfff);
		return true;
	} else {
		access_info info;

		info.type = ACCESS_FETCH;
		info.mode = _cpu.kernel_mode() ? ACCESS_KERNEL : ACCESS_USER;

		if (!resolve_gpa(va, pa, info, fault, true))
			return false;
		
		if (fault == NONE) {
			itlb[cache_idx].tag = va_page;
			itlb[cache_idx].value = pa & ~0xfff;
		}
		
		return true;
	}
}
